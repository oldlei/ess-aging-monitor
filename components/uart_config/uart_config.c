#include "uart_config.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "UART_CONFIG";

static uart_instance_t rs485_inst = {
    .write = rs485_write,
    .read = rs485_read,
    .mutex = NULL,
    .mux_type = MUX_TYPE_RS485,
    .baud_rate = 0,
    .name = "RS485"
};

static uart_instance_t rs232_inst = {
    .write = rs232_write,
    .read = rs232_read,
    .mutex = NULL,
    .mux_type = MUX_TYPE_RS232,
    .baud_rate = 0,
    .name = "RS232"
};

// ==================== 初始化所有串口 ====================
esp_err_t uart_config_init_all(uint32_t baud_rate)
{
    // 创建互斥锁
    rs485_inst.mutex = xSemaphoreCreateMutex();
    rs232_inst.mutex = xSemaphoreCreateMutex();
    if (!rs485_inst.mutex || !rs232_inst.mutex) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_FAIL;
    }

    // 初始化 MUX
    esp_err_t ret = mux_control_init();
    if (ret != ESP_OK) {
        return ret;
    }

    // 初始化 RS485 (UART1)
    ret = rs485_init(baud_rate);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RS485 初始化失败");
        return ret;
    }
    rs485_inst.baud_rate = baud_rate;

    // 初始化 RS232 (UART2)
    ret = rs232_init(baud_rate);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RS232 初始化失败");
        return ret;
    }
    rs232_inst.baud_rate = baud_rate;

    ESP_LOGI(TAG, "所有串口初始化完成 (RS485 + RS232 同时运行)");
    return ESP_OK;
}

// ==================== 获取实例 ====================
uart_instance_t *uart_get_rs485(void) { return &rs485_inst; }
uart_instance_t *uart_get_rs232(void) { return &rs232_inst; }

// ==================== RS485 轮询发送 ====================
int uart_rs485_poll_send(uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (xSemaphoreTake(rs485_inst.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return -1;
    }

    // 切换到下一个通道
    mux_channel_t ch = mux_next_channel(MUX_TYPE_RS485);
    ESP_LOGD(TAG, "RS485 轮询通道 %d", ch);

    // 发送数据
    int sent = rs485_write(data, len);

    // 等待响应
    uint8_t buf[256];
    int recv = rs485_read(buf, sizeof(buf), timeout_ms);

    xSemaphoreGive(rs485_inst.mutex);
    return sent;
}

// ==================== RS232 轮询发送 ====================
int uart_rs232_poll_send(uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (xSemaphoreTake(rs232_inst.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return -1;
    }

    mux_channel_t ch = mux_next_channel(MUX_TYPE_RS232);
    ESP_LOGD(TAG, "RS232 轮询通道 %d", ch);

    int sent = rs232_write(data, len);

    uint8_t buf[256];
    int recv = rs232_read(buf, sizeof(buf), timeout_ms);

    xSemaphoreGive(rs232_inst.mutex);
    return sent;
}
