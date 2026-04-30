//
// Created by 成雷 on 2026/4/29.
//

/**
 * @file poll_cmd.c
 * @brief 轮询命令定义实现
 *
 * 包含轮询命令的实际字节数据定义。
 */

#include "poll_cmd.h"

/**
 * @brief 轮询命令数组
 *
 * 这些命令用于查询 ESS 设备的不同参数：
 * - poll_cmd_1: 获取 SN（序列号）
 * - poll_cmd_2/3/4: 获取其他设备参数
 *
 * @note 每个命令包含 8 字节，格式为 [目标地址][命令类型]...
 */
static const uint8_t poll_cmd_1[] = {0x00, 0x04, 0x17, 0x00, 0x00, 0x33, 0xB4, 0x7A};
static const uint8_t poll_cmd_2[] = {0x00, 0x04, 0x10, 0x00, 0x00, 0x12, 0x75, 0x16};
static const uint8_t poll_cmd_3[] = {0x00, 0x04, 0x11, 0x00, 0x00, 0x1A, 0x75, 0x2C};
static const uint8_t poll_cmd_4[] = {0x00, 0x01, 0x12, 0x00, 0x00, 0x90, 0x38, 0xCF};

const uint8_t* const poll_commands[POLL_CMD_COUNT] = {
    poll_cmd_1, poll_cmd_2, poll_cmd_3, poll_cmd_4,
};

const uint16_t poll_cmd_lengths[POLL_CMD_COUNT] = {
    sizeof(poll_cmd_1), sizeof(poll_cmd_2), sizeof(poll_cmd_3), sizeof(poll_cmd_4),
};
