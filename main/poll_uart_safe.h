//
// Created by 成雷 on 2026/4/29.
//

/**
 * @file poll_uart_safe.h
 * @brief UART 安全操作模块
 *
 * 提供线程安全的 UART 读写接口，通过互斥锁保护临界资源，
 * 避免多个任务同时访问 UART 造成数据冲突。
 */

#ifndef POLL_UART_SAFE_H
#define POLL_UART_SAFE_H

#include <stdint.h>

/**
 * @brief RS485 安全写入
 *
 * 在写入 UART 前获取互斥锁，写入完成后释放。
 *
 * @param data 要发送的数据指针
 * @param len 数据长度
 * @return 实际写入的字节数，失败返回 -1
 */
int rs485_write_safe(uint8_t *data, int len);

/**
 * @brief RS485 安全读取
 *
 * 在读取 UART 前获取互斥锁，读取完成后释放。
 *
 * @param data 接收数据缓冲区
 * @param len 缓冲区最大长度
 * @param timeout_ms 超时时间（毫秒）
 * @return 实际读取的字节数，超时返回 0，失败返回 -1
 */
int rs485_read_safe(uint8_t *data, int len, int timeout_ms);

/**
 * @brief RS232 安全写入
 *
 * @param data 要发送的数据指针
 * @param len 数据长度
 * @return 实际写入的字节数，失败返回 -1
 */
int rs232_write_safe(uint8_t *data, int len);

/**
 * @brief RS232 安全读取
 *
 * @param data 接收数据缓冲区
 * @param len 缓冲区最大长度
 * @param timeout_ms 超时时间（毫秒）
 * @return 实际读取的字节数，超时返回 0，失败返回 -1
 */
int rs232_read_safe(uint8_t *data, int len, int timeout_ms);

#endif  // POLL_UART_SAFE_H
