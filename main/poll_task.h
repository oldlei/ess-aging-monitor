//
// Created by 成雷 on 2026/4/29.
//

/**
 * @file poll_task.h
 * @brief 轮询任务主模块头文件
 *
 * 该模块负责通过 UART (RS485/RS232) 与 ESS 设备通信，轮询各个通道的状态数据，
 * 并将结果通过 MQTT 协议发布到指定主题。
 *
 * @see poll_cmd.h    轮询命令定义
 * @see poll_sn_cache.h SN 缓存管理
 * @see poll_uart_safe.h UART 安全操作
 */

#ifndef ESP32CHUNENG_POLL_TASK_H
#define ESP32CHUNENG_POLL_TASK_H

#include <stdint.h>
#include <stdbool.h>

// SN 更新间隔（10分钟）- 供外部模块使用
#define SN_UPDATE_INTERVAL_MS  600000

/**
 * @brief 回复缓存数据类型
 *
 * 用于获取所有命令的回复数据
 */
typedef char response_array_t[4][400];

/**
 * @brief 定时发送使能标志
 *
 * 当设置为 true 时，轮询任务开始发送命令
 */
extern volatile bool timer_send_enabled;

/**
 * @brief 定时发送间隔（毫秒）
 *
 * 控制两次轮询之间的时间间隔
 */
extern uint32_t timer_send_interval;

/**
 * @brief 初始化并启动轮询任务
 *
 * 创建 FreeRTOS 任务，堆栈大小 8192 字节，优先级 5。
 * 任务名称为 "uart_poll"。
 */
void poll_task_init(void);

/**
 * @brief 强制更新所有通道的 SN
 */
void poll_task_force_update_sn(void);

/**
 * @brief 获取指定通道的回复数据
 *
 * @param channel 通道编号
 * @return 回复数据数组指针
 */
const response_array_t* poll_task_get_responses(uint8_t channel);

#endif  // ESP32CHUNENG_POLL_TASK_H
