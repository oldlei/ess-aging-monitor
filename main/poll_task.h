//
// Created by 成雷 on 2026/4/29.
//

#ifndef ESP32CHUNENG_POLL_TASK_H
#define ESP32CHUNENG_POLL_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"

// 轮询命令数量
#define POLL_CMD_COUNT 4
// 通道数量
#define CHANNEL_COUNT  4
// SN 长度
#define SN_LENGTH      16
// SN 更新间隔（10分钟）
#define SN_UPDATE_INTERVAL_MS  600000

// SN 缓存结构
typedef struct {
    bool valid;
    uint32_t last_update_tick;  // 上次更新时间
    uint8_t sn[SN_LENGTH];
    uint8_t sn_len;
} channel_sn_t;

// 轮询命令（外部可访问）
extern const uint8_t* const poll_commands[POLL_CMD_COUNT];
extern const uint16_t poll_cmd_lengths[POLL_CMD_COUNT];

// 定时发送控制
extern volatile bool timer_send_enabled;
extern uint32_t timer_send_interval;

// 初始化
void poll_task_init(void);

// 强制更新所有通道的 SN
void poll_task_force_update_sn(void);

#endif //ESP32CHUNENG_POLL_TASK_H
