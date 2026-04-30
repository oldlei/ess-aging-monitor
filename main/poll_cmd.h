//
// Created by 成雷 on 2026/4/29.
//

/**
 * @file poll_cmd.h
 * @brief 轮询命令定义
 *
 * 定义与 ESS 设备通信的轮询命令数组和长度。
 */

#ifndef POLL_CMD_H
#define POLL_CMD_H

#include <stdint.h>

/**
 * @brief 轮询命令数量
 */
#define POLL_CMD_COUNT 4

/**
 * @brief 轮询命令指针数组
 *
 * 方便通过索引访问不同的命令
 */
extern const uint8_t* const poll_commands[POLL_CMD_COUNT];

/**
 * @brief 各轮询命令的长度数组
 */
extern const uint16_t poll_cmd_lengths[POLL_CMD_COUNT];

#endif  // POLL_CMD_H
