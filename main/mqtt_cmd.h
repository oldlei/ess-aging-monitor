//
// Created by 成雷 on 2026/4/29.
//

#ifndef ESP32CHUNENG_MQTT_CMD_H
#define ESP32CHUNENG_MQTT_CMD_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "poll_task.h"

// 当前串口模式
typedef enum {
    UART_MODE_RS485 = 0,
    UART_MODE_RS232 = 1,
} uart_mode_t;

extern uart_mode_t current_uart_mode;

// 状态查询
void mqtt_cmd_report_status(void);

// 初始化
void mqtt_cmd_init(void);

#endif //ESP32CHUNENG_MQTT_CMD_H
