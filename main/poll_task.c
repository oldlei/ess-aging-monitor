//
// Created by 成雷 on 2026/4/29.
//
#include "poll_task.h"
#include "mqtt_cmd.h"
#include "esp_log.h"
#include "mux_control.h"
#include "uart_config.h"
#include "mqtt_wrapper.h"
#include <string.h>
#include <stdio.h>

#define TAG "POLL_TASK"
#define RESPONSE_TIMEOUT_MS 500
#define MQTT_TOPIC_RS485 "uart/rs485"
#define MQTT_TOPIC_RS232 "uart/rs232"

// ==================== 轮询命令定义 ====================
static const uint8_t poll_cmd_1[] = {0x00, 0x04, 0x17, 0x00, 0x00, 0x33, 0xB4, 0x7A};
static const uint8_t poll_cmd_2[] = {0x00, 0x04, 0x10, 0x00, 0x00, 0x12, 0x75, 0x16};
static const uint8_t poll_cmd_3[] = {0x00, 0x04, 0x11, 0x00, 0x00, 0x1A, 0x75, 0x2C};
static const uint8_t poll_cmd_4[] = {0x00, 0x01, 0x12, 0x00, 0x00, 0x90, 0x38, 0xCF};

const uint8_t* const poll_commands[POLL_CMD_COUNT] = {
    poll_cmd_1, poll_cmd_2, poll_cmd_3, poll_cmd_4,
};
const uint16_t poll_cmd_lengths[POLL_CMD_COUNT] = {
    sizeof(poll_cmd_1), sizeof(poll_cmd_2), sizeof(poll_cmd_3), sizeof(poll_cmd_4),
};

// 定时发送控制
volatile bool timer_send_enabled = false;
uint32_t timer_send_interval = 1000;

// ==================== SN 缓存 ====================
static channel_sn_t sn_cache[CHANNEL_COUNT] = {0};

// 回复数据缓存
static char cmd_response[CHANNEL_COUNT][POLL_CMD_COUNT][400];

// 强制更新标志
static bool force_sn_update = false;

// ==================== UART 安全操作宏 ====================
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

// ==================== SN 缓存管理 ====================
void poll_task_init_sn_cache(void)
{
    memset(sn_cache, 0, sizeof(sn_cache));
    memset(cmd_response, 0, sizeof(cmd_response));
    force_sn_update = false;
    ESP_LOGI(TAG, "SN 缓存已初始化");
}

void poll_task_force_update_sn(void)
{
    force_sn_update = true;
    ESP_LOGI(TAG, "强制更新 SN 标志已设置");
}

static bool need_update_sn(uint8_t channel)
{
    if (force_sn_update) return true;
    if (!sn_cache[channel].valid) return true;

    uint32_t elapsed = (xTaskGetTickCount() - sn_cache[channel].last_update_tick)
                       * portTICK_PERIOD_MS;
    return (elapsed >= SN_UPDATE_INTERVAL_MS);
}

// 解析 SN（根据实际协议修改偏移量）
// TODO: 根据实际协议修改 SN 位置和长度
static bool parse_sn(uint8_t channel, const uint8_t *data, int len)
{
    // 假设 SN 在 data[8] ~ data[15]，共 8 字节
    // TODO: 根据实际协议修改此处
    if (len < 16) return false;

    memcpy(sn_cache[channel].sn, &data[8], 8);
    sn_cache[channel].sn_len = 8;
    sn_cache[channel].valid = true;
    sn_cache[channel].last_update_tick = xTaskGetTickCount();

    return true;
}

// 十六进制转字符串
static void bytes_to_hex_str(const uint8_t *data, int len, char *out, int out_len)
{
    int pos = 0;
    for (int i = 0; i < len && pos < out_len - 3; i++) {
        pos += snprintf(out + pos, out_len - pos, "%02X", data[i]);
        if (i < len - 1) out[pos++] = ' ';
    }
}

// ==================== 统一轮询任务 ====================
static void poll_task(void *pvParameters)
{
    uint8_t rx_buffer[512]; // todo 修改了这里 256-》512
    uint8_t tx_buffer[16];

    poll_task_init_sn_cache();

    while (1) {
        if (timer_send_enabled) {
            // 根据当前 UART 模式选择 MUX 类型和 MQTT 主题
            mux_type_t mux_type = (current_uart_mode == UART_MODE_RS485) ? MUX_TYPE_RS485 : MUX_TYPE_RS232;
            const char *mqtt_topic = (current_uart_mode == UART_MODE_RS485) ? MQTT_TOPIC_RS485 : MQTT_TOPIC_RS232;
            const char *mode_name = (current_uart_mode == UART_MODE_RS485) ? "RS485" : "RS232";

            mux_channel_t ch = mux_next_channel(mux_type);
            ESP_LOGI(TAG, "%s 轮询通道 %d", mode_name, ch);

            // 清空该通道的回复缓存
            memset(cmd_response[ch], 0, sizeof(cmd_response[ch]));

            // RS485 模式：检查是否需要更新 SN
            // RS232 模式：不需要 SN
            bool do_sn_update = (current_uart_mode == UART_MODE_RS485) && need_update_sn(ch);

            for (int cmd_idx = 0; cmd_idx < POLL_CMD_COUNT; cmd_idx++) {
                // RS485 poll_cmd_1：检查 SN 缓存
                if (current_uart_mode == UART_MODE_RS485 && cmd_idx == 0) {
                    if (!do_sn_update) {
                        ESP_LOGI(TAG, "RS485 CH%d 跳过 poll_cmd_1 (SN有效)", ch);
                        continue;  // 跳过，保留之前的回复
                    }
                }

                // 准备发送数据
                memcpy(tx_buffer, poll_commands[cmd_idx], poll_cmd_lengths[cmd_idx]);
                int tx_len = poll_cmd_lengths[cmd_idx];

                // RS485 需要附加通道号
                if (current_uart_mode == UART_MODE_RS485) {
                    tx_buffer[tx_len] = (uint8_t)ch;
                    tx_len++;
                }

                // 发送命令
                int sent_len;
                int recv_len;
                if (current_uart_mode == UART_MODE_RS485) {
                    sent_len = rs485_write_safe(tx_buffer, tx_len);
                    if (sent_len > 0) {
                        ESP_LOGI(TAG, "RS485 CH%d 发送命令%d", ch, cmd_idx + 1);
                        recv_len = rs485_read_safe(rx_buffer, sizeof(rx_buffer), RESPONSE_TIMEOUT_MS);
                    } else {
                        recv_len = -1;
                    }
                } else {
                    sent_len = rs232_write_safe(tx_buffer, tx_len);
                    if (sent_len > 0) {
                        ESP_LOGI(TAG, "RS232 CH%d 发送命令%d", ch, cmd_idx + 1);
                        recv_len = rs232_read_safe(rx_buffer, sizeof(rx_buffer), RESPONSE_TIMEOUT_MS);
                    } else {
                        recv_len = -1;
                    }
                }

                if (sent_len > 0) {
                    if (recv_len > 0) {
                        // 转换为十六进制字符串并保存
                        bytes_to_hex_str(rx_buffer, recv_len, cmd_response[ch][cmd_idx], 256);

                        ESP_LOGI(TAG, "%s CH%d 命令%d 收到 [%d]: %s",
                                  mode_name, ch, cmd_idx + 1, recv_len, cmd_response[ch][cmd_idx]);
                        ESP_LOGI(TAG, "%s CH%d 命令%d 收到 [%d]: %s (bufsize=%d)",
                          mode_name, ch, cmd_idx + 1, recv_len, cmd_response[ch][cmd_idx],
                          (int)sizeof(cmd_response[ch][cmd_idx]));
                        printf("%s",cmd_response[ch][cmd_idx]);


                        // RS485：解析 SN（poll_cmd_1 回复）
                        if (current_uart_mode == UART_MODE_RS485 && cmd_idx == 0) {
                            if (parse_sn(ch, rx_buffer, recv_len)) {
                                ESP_LOGI(TAG, "RS485 CH%d SN已更新", ch);
                            }
                        }
                    } else {
                        snprintf(cmd_response[ch][cmd_idx], 256, "TIMEOUT");
                        ESP_LOGW(TAG, "%s CH%d 命令%d 超时", mode_name, ch, cmd_idx + 1);
                    }
                } else {
                    snprintf(cmd_response[ch][cmd_idx], 256, "SEND_FAIL");
                    ESP_LOGW(TAG, "%s CH%d 命令%d 发送失败", mode_name, ch, cmd_idx + 1);
                }
            }

            // RS485 模式：清除强制更新标志
            if (do_sn_update) {
                force_sn_update = false;
            }

            // 发送 MQTT 消息 (RS232 模式需要更大的缓冲区)
            char mqtt_payload[2200];
            if (current_uart_mode == UART_MODE_RS485) {
                // RS485 模式：包含 SN
                char sn_hex[64] = "";
                if (sn_cache[ch].valid) {
                    bytes_to_hex_str(sn_cache[ch].sn, sn_cache[ch].sn_len, sn_hex, sizeof(sn_hex));
                }

                snprintf(mqtt_payload, sizeof(mqtt_payload),
                         "{\"ch\":%d,\"SN\":\"%s\",\"cmd1\":\"%s\",\"cmd2\":\"%s\",\"cmd3\":\"%s\",\"cmd4\":\"%s\"}",
                         ch,
                         sn_hex,
                         cmd_response[ch][0],
                         cmd_response[ch][1],
                         cmd_response[ch][2],
                         cmd_response[ch][3]);
            } else {
                // RS232 模式：无 SN
                snprintf(mqtt_payload, sizeof(mqtt_payload),
                         "{\"ch\":%d,\"cmd1\":\"%s\",\"cmd2\":\"%s\",\"cmd3\":\"%s\",\"cmd4\":\"%s\"}",
                         ch,
                         cmd_response[ch][0],
                         cmd_response[ch][1],
                         cmd_response[ch][2],
                         cmd_response[ch][3]);
            }

            mqtt_wrapper_publish(mqtt_topic, mqtt_payload, strlen(mqtt_payload));
            ESP_LOGI(TAG, "%s CH%d MQTT已发布", mode_name, ch);

            ESP_LOGI(TAG, "%s CH%d 完成，等待 %lums", mode_name, ch, timer_send_interval);
            vTaskDelay(pdMS_TO_TICKS(timer_send_interval));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// ==================== 初始化 ====================
void poll_task_init(void)
{
    xTaskCreate(poll_task, "uart_poll", 8192, NULL, 5, NULL);
    ESP_LOGI(TAG, "统一轮询任务已启动");
}
