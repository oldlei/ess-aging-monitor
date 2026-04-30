#ifndef RS232_H
#define RS232_H

#include "esp_err.h"
#include "driver/gpio.h"

// ==================== 配置 ====================
#define RS232_UART_PORT    UART_NUM_2
#define RS232_TX_PIN       GPIO_NUM_17
#define RS232_RX_PIN       GPIO_NUM_18

// ==================== 对外接口 ====================
/**
 * @brief  初始化 RS232 接口
 * @param  baud_rate 波特率
 * @return esp_err_t
 */
esp_err_t rs232_init(uint32_t baud_rate);

/**
 * @brief  从 RS232 读取数据
 * @param  data 数据缓冲区
 * @param  len  读取长度
 * @param  timeout_ms 超时时间(ms)
 * @return 实际读取的字节数
 */
int rs232_read(uint8_t *data, size_t len, uint32_t timeout_ms);

/**
 * @brief  向 RS232 写入数据
 * @param  data 数据缓冲区
 * @param  len  写入长度
 * @return 实际写入的字节数
 */
int rs232_write(uint8_t *data, size_t len);

#endif // RS232_H