#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rgb_led.h"
#include "mqtt_wrapper.h"
#include "esp_log.h"
#include "wifi_smartconfig.h"
#include "rs485.h"
#include "uart_config.h"




// ==================== 配置 ====================
#define TAG                 "MAIN"
#define UART_BAUD_RATE      115200
#define RESPONSE_TIMEOUT_MS  500  // 接收超时时间(ms)

// ==================== MQTT 配置 ====================
// MQTT 代理服务器地址
#define MQTT_BROKER_URI     "mqtt://192.168.4.6"
// MQTT 控制主题
// 用于接收控制命令，订阅此主题来获取控制指令
#define MQTT_TOPIC_CTRL     "uart/control"
// MQTT 定时发送主题
// 用于接收定时发送控制命令
#define MQTT_TOPIC_TIMER    "uart/timer"
// MQTT 状态主题
// 用于发布状态信息和接收到的 RS485 数据
#define MQTT_TOPIC_STATUS   "uart/status"

// MQTT 状态查询主题
// 用于查询当前状态，发送任何消息都会返回状态信息
#define MQTT_TOPIC_QUERY    "uart/status/query"

// 添加串口切换主题
// #define MQTT_TOPIC_UART_SWITCH  "uart/switch"


// ==================== 串口数据类型 ====================
typedef enum {
DATA_TYPE_HELLO = 0,
DATA_TYPE_SENSOR,
DATA_TYPE_CMD,
DATA_TYPE_CUSTOM,
} uart_data_type_t;

// ==================== 当前串口类型 ====================
typedef enum {
UART_MODE_RS485 = 0,  // 当前使用 RS485
UART_MODE_RS232 = 1,  // 当前使用 RS232
} uart_mode_t;

static uart_mode_t current_uart_mode = UART_MODE_RS485;  // 默认 RS485



// 预定义数据类型
static const uint8_t data_hello[] = "Hello RS485!\n";
static const uint8_t data_sensor[] = "SENSOR:TEMP:25.5,HUM:60.2\n";
static const uint8_t data_cmd[] = "CMD:DEVICE:ON,MODE:1\n";
static const uint8_t data_custom[] = "CUSTOM:DATA:12345678\n";

// ==================== 轮询命令定义 ====================
// 轮询时每通道发送的4条命令（十六进制格式）
typedef enum {
POLL_CMD_1 = 0,
POLL_CMD_2 = 1,
POLL_CMD_3 = 2,
POLL_CMD_4 = 3,
POLL_CMD_COUNT = 4,
} poll_cmd_index_t;

static const uint8_t poll_cmd_1[] = {0x00, 0x04, 0x17, 0x00, 0x00, 0x33, 0xB4, 0x7A};
static const uint8_t poll_cmd_2[] = {0x00, 0x04, 0x10, 0x00, 0x00, 0x12, 0x75, 0x16};
static const uint8_t poll_cmd_3[] = {0x00, 0x04, 0x11, 0x00, 0x00, 0x1A, 0x75, 0x2C};
static const uint8_t poll_cmd_4[] = {0x00, 0x01, 0x12, 0x00, 0x00, 0x90, 0x38, 0xCF};

// 轮询命令数组
static const uint8_t* const poll_commands[POLL_CMD_COUNT] = {
poll_cmd_1,
poll_cmd_2,
poll_cmd_3,
poll_cmd_4,
};
static const uint16_t poll_cmd_lengths[POLL_CMD_COUNT] = {
sizeof(poll_cmd_1),
sizeof(poll_cmd_2),
sizeof(poll_cmd_3),
sizeof(poll_cmd_4),
};

static uart_data_type_t timer_send_data_type = DATA_TYPE_HELLO;//定时发送的数据类型

// 定时发送控制变量
static bool timer_send_enabled = false;
static uint32_t timer_send_interval = 1000; // 默认 1 秒
#define rs485_write_safe(data, len) ({ \
int _r = -1; \
uart_instance_t *_inst = uart_get_rs485(); \
if (_inst->mutex && xSemaphoreTake(_inst->mutex, pdMS_TO_TICKS(100)) == pdTRUE) { \
_r = _inst->write((data), (len)); \
xSemaphoreGive(_inst->mutex); \
} \
_r; \
})


#define rs485_read_safe(data, len, to) ({ \
int _r = -1; \
uart_instance_t *_inst = uart_get_rs485(); \
if (_inst->mutex && xSemaphoreTake(_inst->mutex, pdMS_TO_TICKS(100)) == pdTRUE) { \
_r = _inst->read((data), (len), (to)); \
xSemaphoreGive(_inst->mutex); \
} \
_r; \
})
// RS232 操作
#define rs232_write_safe(data, len) ({ \
int _r = -1; \
uart_instance_t *_inst = uart_get_rs232(); \
if (_inst->mutex && xSemaphoreTake(_inst->mutex, pdMS_TO_TICKS(100)) == pdTRUE) { \
_r = _inst->write((data), (len)); \
xSemaphoreGive(_inst->mutex); \
} \
_r; \
})

#define rs232_read_safe(data, len, to) ({ \
int _r = -1; \
uart_instance_t *_inst = uart_get_rs232(); \
if (_inst->mutex && xSemaphoreTake(_inst->mutex, pdMS_TO_TICKS(100)) == pdTRUE) { \
_r = _inst->read((data), (len), (to)); \
xSemaphoreGive(_inst->mutex); \
} \
_r; \
})
// #define uart_name() uart_config_get()->name
#define rs485_name()  uart_get_rs485()->name
#define rs232_name()  uart_get_rs232()->name




// 状态查询函数
static void report_status(void)
{
char status_msg[128];

    const char *uart_mode_str = (current_uart_mode == UART_MODE_RS485) ? "RS485" : "RS232";

    snprintf(status_msg, sizeof(status_msg),
         "STATUS:UART=%s,CH=%d,TIMER=%s,INT=%lu",
         uart_mode_str,
         mux_get_channel(current_uart_mode == UART_MODE_RS485 ? MUX_TYPE_RS485 : MUX_TYPE_RS232),
         timer_send_enabled ? "RUNNING" : "STOPPED",
         timer_send_interval);
    ESP_LOGI(TAG, "状态: %s", status_msg);
    mqtt_wrapper_publish(MQTT_TOPIC_STATUS, status_msg, strlen(status_msg));
}


// ==================== MQTT 状态查询回调 ====================
static void mqtt_query_callback(const char *topic, const char *data, size_t len)
{
ESP_LOGI(TAG, "收到状态查询请求");
report_status();
}

// ==================== MQTT 串口模式切换回调 ====================
static void mqtt_uart_mode_callback(const char *topic, const char *data, size_t len)
{
ESP_LOGI(TAG, "收到串口切换指令: %s", data);

    if (len <= 0) return;

    char cmd = data[0];

    if (cmd == 'R' || cmd == 'r') {
        // 切换到 RS485
        current_uart_mode = UART_MODE_RS485;
        ESP_LOGI(TAG, "已切换到 RS485 模式");
        char status_msg[64];
        snprintf(status_msg, sizeof(status_msg), "UART_MODE:RS485");
        mqtt_wrapper_publish(MQTT_TOPIC_STATUS, status_msg, strlen(status_msg));
    } else if (cmd == 'S' || cmd == 's') {
        // 切换到 RS232
        current_uart_mode = UART_MODE_RS232;
        ESP_LOGI(TAG, "已切换到 RS232 模式");
        char status_msg[64];
        snprintf(status_msg, sizeof(status_msg), "UART_MODE:RS232");
        mqtt_wrapper_publish(MQTT_TOPIC_STATUS, status_msg, strlen(status_msg));
    } else {
        ESP_LOGW(TAG, "未知串口切换命令: %c (使用 R=RS485, S=RS232)", cmd);
    }
}


static void mqtt_ctrl_callback(const char *topic, const char *data, size_t len)
{
ESP_LOGI(TAG, "收到控制数据: %s", data);

    if (len <= 0) return;

    // 根据当前串口模式确定目标 MUX
    mux_type_t target_mux = (current_uart_mode == UART_MODE_RS485) ? MUX_TYPE_RS485 : MUX_TYPE_RS232;
    mux_channel_t target_ch = mux_get_channel(target_mux);
    uart_data_type_t data_type;
    const uint8_t *data_ptr = NULL;
    size_t data_len = 0;

    // 解析新格式: R/S + 通道号 + 数据类型
    if (len >= 3 && (data[0] == 'R' || data[0] == 'S')) {
        target_mux = (data[0] == 'R') ? MUX_TYPE_RS485 : MUX_TYPE_RS232;
        target_ch = (data[1] - '0');
        char cmd = data[2];

        if (target_ch > 3) {
            ESP_LOGW(TAG, "无效通道: %d", target_ch);
            return;
        }

        switch (cmd) {
        case '0': data_type = DATA_TYPE_HELLO;   data_ptr = data_hello;   data_len = sizeof(data_hello)-1;   break;
        case '1': data_type = DATA_TYPE_SENSOR;  data_ptr = data_sensor;  data_len = sizeof(data_sensor)-1;  break;
        case '2': data_type = DATA_TYPE_CMD;     data_ptr = data_cmd;     data_len = sizeof(data_cmd)-1;     break;
        case '3': data_type = DATA_TYPE_CUSTOM;  data_ptr = data_custom;  data_len = sizeof(data_custom)-1;  break;
        default:
            ESP_LOGW(TAG, "未知数据类型: %c", cmd);
            return;
        }
    }
    // 兼容旧格式: 单字符 '0'~'3'
    else {
        char cmd = data[0];
        switch (cmd) {
        case '0': data_type = DATA_TYPE_HELLO;   data_ptr = data_hello;   data_len = sizeof(data_hello)-1;   break;
        case '1': data_type = DATA_TYPE_SENSOR;  data_ptr = data_sensor;  data_len = sizeof(data_sensor)-1;  break;
        case '2': data_type = DATA_TYPE_CMD;     data_ptr = data_cmd;     data_len = sizeof(data_cmd)-1;     break;
        case '3': data_type = DATA_TYPE_CUSTOM;  data_ptr = data_custom;  data_len = sizeof(data_custom)-1;  break;
        default:
            ESP_LOGW(TAG, "未知命令: %c", cmd);
            return;
        }
    }

    // 切换 MUX 通道
    mux_select_channel(target_mux, target_ch);

    // 根据目标类型选择对应的 UART 发送
    int sent_len;
    const char *type_name;
    if (target_mux == MUX_TYPE_RS485) {
        uart_instance_t *inst = uart_get_rs485();
        if (xSemaphoreTake(inst->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            sent_len = inst->write((uint8_t*)data_ptr, data_len);
            xSemaphoreGive(inst->mutex);
        } else {
            sent_len = -1;
        }
        type_name = "RS485";
    } else {
        uart_instance_t *inst = uart_get_rs232();
        if (xSemaphoreTake(inst->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            sent_len = inst->write((uint8_t*)data_ptr, data_len);
            xSemaphoreGive(inst->mutex);
        } else {
            sent_len = -1;
        }
        type_name = "RS232";
    }

    if (sent_len > 0) {
        ESP_LOGI(TAG, "已发送到 %s 通道%d, 类型=%d, 长度=%d", type_name, target_ch, data_type, sent_len);

        char status_msg[64];
        snprintf(status_msg, sizeof(status_msg), "OK:%s_CH%d:TYPE=%d,LEN=%d", type_name, target_ch, data_type, sent_len);
        mqtt_wrapper_publish(MQTT_TOPIC_STATUS, status_msg, strlen(status_msg));

        timer_send_data_type = data_type;
    }
}
// ==================== MQTT 定时发送回调 ====================
static void mqtt_timer_callback(const char *topic, const char *data, size_t len)
{
ESP_LOGI(TAG, "收到定时器数据: %s", data);

    if (len > 0) {
        char cmd = data[0];

        // 启动定时发送
        if (cmd == '4') {
            timer_send_enabled = true;
            ESP_LOGI(TAG, "定时发送已启动");
            char status_msg[64];
            snprintf(status_msg, sizeof(status_msg), "TIMER:STARTED,INT=%lu", timer_send_interval);
            mqtt_wrapper_publish(MQTT_TOPIC_STATUS, status_msg, strlen(status_msg));
            return;
        }

        // 停止定时发送
        if (cmd == '5') {
            timer_send_enabled = false;
            ESP_LOGI(TAG, "定时发送已停止");
            char status_msg[64];
            snprintf(status_msg, sizeof(status_msg), "TIMER:STOPPED");
            mqtt_wrapper_publish(MQTT_TOPIC_STATUS, status_msg, strlen(status_msg));
            return;
        }

        // 设置发送间隔（格式：6,1000 表示设置为 1000ms）
        if (cmd == '6' && len > 2 && data[1] == ',') {

            uint32_t new_interval = (uint32_t)atoi(data + 2);
            if (new_interval > 100 && new_interval < 60000) {
                timer_send_interval = new_interval;
                ESP_LOGI(TAG, "定时发送间隔已设置为 %lums", new_interval);
                char status_msg[64];
                snprintf(status_msg, sizeof(status_msg), "TIMER:INTERVAL=%lu", new_interval);
                mqtt_wrapper_publish(MQTT_TOPIC_STATUS, status_msg, strlen(status_msg));
            } else {
                ESP_LOGW(TAG, "无效的间隔值: %lu (范围: 100-60000)", new_interval);
            }
            return;
        }

        ESP_LOGW(TAG, "未知定时器命令: %c", cmd);
    }
}

// ==================== MQTT 数据分发回调 ====================
// MQTT 串口模式切换主题
#define MQTT_TOPIC_UART_MODE   "uart/mode"

static void mqtt_data_callback(const char *topic, const char *data, size_t len)
{
ESP_LOGI(TAG, "收到数据, topic=%s, data=%s", topic, data);
if (strcmp(topic, MQTT_TOPIC_CTRL) == 0) {
mqtt_ctrl_callback(topic, data, len);
} else if (strcmp(topic, MQTT_TOPIC_TIMER) == 0) {
mqtt_timer_callback(topic, data, len);
} else if (strcmp(topic, MQTT_TOPIC_QUERY) == 0) {
mqtt_query_callback(topic, data, len);
} else if (strcmp(topic, MQTT_TOPIC_UART_MODE) == 0) {
mqtt_uart_mode_callback(topic, data, len);
}
else {
ESP_LOGW(TAG, "未知的主题: %s", topic);
}
}



// RS485 轮询任务（仅在 RS485 模式下执行定时发送）
void rs485_poll_task(void *pvParameters)
{
uint8_t rx_buffer[256];
uint8_t tx_buffer[16];  // 命令 + 通道号  暂时使用
while (1) {
// 只在 RS485 模式下执行定时发送
if (timer_send_enabled && current_uart_mode == UART_MODE_RS485) {
mux_channel_t ch = mux_next_channel(MUX_TYPE_RS485);
ESP_LOGI(TAG, "RS485 轮询通道 %d", ch);

            // 在当前通道发送4条轮询命令
            for (int cmd_idx = 0; cmd_idx < POLL_CMD_COUNT; cmd_idx++) {
                // 复制命令并追加通道号
                memcpy(tx_buffer, poll_commands[cmd_idx], poll_cmd_lengths[cmd_idx]);
                tx_buffer[poll_cmd_lengths[cmd_idx]] = (uint8_t)ch;  // 追加通道号
                int tx_len = poll_cmd_lengths[cmd_idx] + 1;
                int sent_len = rs485_write_safe(tx_buffer, tx_len);
                // 发送命令
                // int sent_len = rs485_write_safe((uint8_t*)poll_commands[cmd_idx], poll_cmd_lengths[cmd_idx]);
                if (sent_len > 0) {
                    ESP_LOGI(TAG, "RS485 CH%d 发送命令%d, 长度=%d", ch, cmd_idx + 1, sent_len);

                    // 等待接收回复
                    int recv_len = rs485_read_safe(rx_buffer, sizeof(rx_buffer), RESPONSE_TIMEOUT_MS);
                    if (recv_len > 0) {
                        // 十六进制打印
                        char hex_str[512];
                        int hex_pos = 0;
                        for (int i = 0; i < recv_len && hex_pos < (int)sizeof(hex_str) - 4; i++) {
                            hex_pos += snprintf(hex_str + hex_pos, sizeof(hex_str) - hex_pos, "%02X ", rx_buffer[i]);
                        }
                        ESP_LOGI(TAG, "RS485 CH%d 命令%d 收到回复 [%d]: %s", ch, cmd_idx + 1, recv_len, hex_str);
                    } else {
                        ESP_LOGW(TAG, "RS485 CH%d 命令%d 超时无回复", ch, cmd_idx + 1);
                    }
                } else {
                    ESP_LOGW(TAG, "RS485 CH%d 命令%d 发送失败", ch, cmd_idx + 1);
                }
            }

            ESP_LOGI(TAG, "RS485 CH%d 轮询完成，等待间隔 %lums", ch, timer_send_interval);
            vTaskDelay(pdMS_TO_TICKS(timer_send_interval));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// RS232 轮询任务（仅在 RS232 模式下执行定时发送）
void rs232_poll_task(void *pvParameters)
{
uint8_t rx_buffer[256];

    while (1) {
        // 只在 RS232 模式下执行定时发送
        if (timer_send_enabled && current_uart_mode == UART_MODE_RS232) {
            mux_channel_t ch = mux_next_channel(MUX_TYPE_RS232);
            ESP_LOGI(TAG, "RS232 轮询通道 %d", ch);

            // 在当前通道发送4条轮询命令
            for (int cmd_idx = 0; cmd_idx < POLL_CMD_COUNT; cmd_idx++) {
                // 发送命令
                int sent_len = rs232_write_safe((uint8_t*)poll_commands[cmd_idx], poll_cmd_lengths[cmd_idx]);
                if (sent_len > 0) {
                    ESP_LOGI(TAG, "RS232 CH%d 发送命令%d, 长度=%d", ch, cmd_idx + 1, sent_len);

                    // 等待接收回复
                    int recv_len = rs232_read_safe(rx_buffer, sizeof(rx_buffer), RESPONSE_TIMEOUT_MS);
                    if (recv_len > 0) {
                        rx_buffer[recv_len] = '\0';
                        ESP_LOGI(TAG, "RS232 CH%d 收到回复: %s", ch, rx_buffer);
                    } else {
                        ESP_LOGW(TAG, "RS232 CH%d 命令%d 超时无回复", ch, cmd_idx + 1);
                    }
                } else {
                    ESP_LOGW(TAG, "RS232 CH%d 命令%d 发送失败", ch, cmd_idx + 1);
                }
            }

            ESP_LOGI(TAG, "RS232 CH%d 轮询完成，等待间隔 %lums", ch, timer_send_interval);
            vTaskDelay(pdMS_TO_TICKS(timer_send_interval));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}


void app_main(void)
{
ESP_LOGI(TAG, "主程序启动...");

    // 初始化 WiFi SmartConfig
    esp_err_t ret = wifi_smartconfig_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi SmartConfig 初始化失败");
        return;
    }
    ESP_LOGI(TAG, "WiFi SmartConfig 初始化成功，等待配网...");

    // 等待 WiFi 连接
    ESP_LOGI(TAG, "等待 WiFi 连接...");
    while (!wifi_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG, "WiFi 已连接");

    // 初始化串口（根据配置选择 RS485 或 RS232）
    // ret = uart_config_init(UART_TYPE_DEFAULT, UART_BAUD_RATE);
    ret = uart_config_init_all(UART_BAUD_RATE);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "串口初始化失败");
        return;
    }
    // ESP_LOGI(TAG, "串口初始化成功: %s, 波特率: %d", uart_name(), UART_BAUD_RATE);
    ESP_LOGI(TAG, "串口初始化成功: 波特率: %d", UART_BAUD_RATE);


    // 初始化 MQTT 客户端
    ret = mqtt_wrapper_init(MQTT_BROKER_URI);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MQTT 客户端初始化失败");
        return;
    }
    ESP_LOGI(TAG, "MQTT 客户端初始化完成");

    // 注册数据回调
    mqtt_wrapper_set_callback(mqtt_data_callback);

    // 等待 MQTT 连接
    ESP_LOGI(TAG, "等待 MQTT 连接...");
    while (!mqtt_wrapper_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG, "MQTT 已连接");

    // 订阅控制主题
    mqtt_wrapper_subscribe(MQTT_TOPIC_CTRL, 0);
    mqtt_wrapper_subscribe(MQTT_TOPIC_TIMER, 0);
    mqtt_wrapper_subscribe(MQTT_TOPIC_QUERY, 0);
    mqtt_wrapper_subscribe(MQTT_TOPIC_UART_MODE, 0);

    // 创建uart定时发送任务
    xTaskCreate(rs485_poll_task, "rs485_poll", 4096, NULL, 5, NULL);
    xTaskCreate(rs232_poll_task, "rs232_poll", 4096, NULL, 5, NULL);


    ESP_LOGI(TAG, "系统运行中");
    ESP_LOGI(TAG, "MQTT 主题:");
    ESP_LOGI(TAG, "  %s (控制)", MQTT_TOPIC_CTRL);
    ESP_LOGI(TAG, "  %s (定时)", MQTT_TOPIC_TIMER);
    ESP_LOGI(TAG, "  %s (查询)", MQTT_TOPIC_QUERY);
    ESP_LOGI(TAG, "  %s (状态)", MQTT_TOPIC_STATUS);
    ESP_LOGI(TAG, "  %s (串口切换)", MQTT_TOPIC_UART_MODE);
    ESP_LOGI(TAG, "控制命令: '0'=Hello, '1'=Sensor, '2'=Command, '3'=Custom");
    ESP_LOGI(TAG, "定时命令: '4'=启动, '5'=停止, '6,INTERVAL'=设置间隔");
    ESP_LOGI(TAG, "串口切换: 'R'=RS485, 'S'=RS232");
    ESP_LOGI(TAG, "默认模式: RS485，定时发送只对当前模式有效");


}
