#include "rs232.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "driver/uart.h"

// ==================== 配置 ====================
#define TAG                 "RS232"

// ==================== 实现 ====================
/**
 * @brief  初始化 RS232 接口
 * @param  baud_rate 波特率
 * @return esp_err_t
 */
esp_err_t rs232_init(uint32_t baud_rate)
{
    // 配置 UART
    uart_config_t uart_config = {
        .baud_rate =  (int32_t)baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    // 先尝试删除旧驱动（如果存在）
    uart_driver_delete(RS232_UART_PORT);
    // 安装 UART 驱动
    esp_err_t ret = uart_driver_install(RS232_UART_PORT, 1024, 1024, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART 驱动安装失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置 UART
    ret = uart_param_config(RS232_UART_PORT, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART 配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 设置 UART 引脚
    ret = uart_set_pin(RS232_UART_PORT, RS232_TX_PIN, RS232_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART 引脚设置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "RS232 初始化完成，波特率: %d", (int)baud_rate);
    return ESP_OK;
}

/**
 * @brief  从 RS232 读取数据
 * @param  data 数据缓冲区
 * @param  len  读取长度
 * @param  timeout_ms 超时时间(ms)
 * @return 实际读取的字节数
 */
int rs232_read(uint8_t *data, size_t len, uint32_t timeout_ms)
{
    // 读取数据
    int read_len = uart_read_bytes(RS232_UART_PORT, data, len, pdMS_TO_TICKS(timeout_ms));
    if (read_len > 0) {
        ESP_LOGD(TAG, "读取 %d 字节数据", read_len);
    }
    return read_len;
}

/**
 * @brief  向 RS232 写入数据
 * @param  data 数据缓冲区
 * @param  len  写入长度
 * @return 实际写入的字节数
 */
int rs232_write(uint8_t *data, size_t len)
{
    // 写入数据
    int write_len = uart_write_bytes(RS232_UART_PORT, (const char *)data, len);
    
    // 等待发送完成
    uart_wait_tx_done(RS232_UART_PORT, pdMS_TO_TICKS(100));
    
    if (write_len > 0) {
        ESP_LOGD(TAG, "写入 %d 字节数据", write_len);
    }
    return write_len;
}