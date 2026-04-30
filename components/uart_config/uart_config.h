#ifndef UART_CONFIG_H
#define UART_CONFIG_H

#include "esp_err.h"
#include "rs485.h"
#include "rs232.h"
#include "mux_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// ==================== 串口操作函数指针 ====================
typedef int (*uart_write_func_t)(uint8_t *data, size_t len);
typedef int (*uart_read_func_t)(uint8_t *data, size_t len, uint32_t timeout_ms);

// ==================== 串口配置结构体 ====================
typedef struct {
    uart_write_func_t write;
    uart_read_func_t read;
    SemaphoreHandle_t mutex;      // 每个UART独立的互斥锁
    mux_type_t mux_type;          // 关联的MUX类型
    uint32_t baud_rate;
    const char *name;
} uart_instance_t;

// ==================== 对外接口 ====================

/**
 * @brief  初始化所有串口（RS485 + RS232 同时启动）
 */
esp_err_t uart_config_init_all(uint32_t baud_rate);

/**
 * @brief  获取RS485串口实例
 */
uart_instance_t *uart_get_rs485(void);

/**
 * @brief  获取RS232串口实例
 */
uart_instance_t *uart_get_rs232(void);

/**
 * @brief  RS485轮询：切换到下一个通道并发送数据
 */
int uart_rs485_poll_send(uint8_t *data, size_t len, uint32_t timeout_ms);

/**
 * @brief  RS232轮询：切换到下一个通道并发送数据
 */
int uart_rs232_poll_send(uint8_t *data, size_t len, uint32_t timeout_ms);

#endif // UART_CONFIG_H
