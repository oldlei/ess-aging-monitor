#ifndef RS485_H
#define RS485_H

#include "esp_err.h"
#include "driver/gpio.h"

// ==================== 配置 ====================
#define RS485_UART_PORT    UART_NUM_1
#define RS485_TX_PIN       GPIO_NUM_10
#define RS485_RX_PIN       GPIO_NUM_11
// 自动方向切换模块，无需方向控制引脚

// ==================== 对外接口 ====================
/**
 * @brief  初始化 RS485 接口
 * @param  baud_rate 波特率
 * @return esp_err_t
 */
esp_err_t rs485_init(uint32_t baud_rate);

/**
 * @brief  从 RS485 读取数据
 * @param  data 数据缓冲区
 * @param  len  读取长度
 * @param  timeout_ms 超时时间(ms)
 * @return 实际读取的字节数
 */
int rs485_read(uint8_t *data, size_t len, uint32_t timeout_ms);

/**
 * @brief  向 RS485 写入数据
 * @param  data 数据缓冲区
 * @param  len  写入长度
 * @return 实际写入的字节数
 */
int rs485_write(uint8_t *data, size_t len);

#endif // RS485_H