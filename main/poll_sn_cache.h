//
// Created by 成雷 on 2026/4/29.
//

/**
 * @file poll_sn_cache.h
 * @brief SN 缓存管理模块
 *
 * 负责管理 ESS 设备序列号（SN）的缓存，包括：
 * - SN 缓存的初始化和更新
 * - SN 缓存有效期检查
 * - 从设备回复中解析 SN
 * - 字节数据与十六进制字符串的相互转换
 */

#ifndef POLL_SN_CACHE_H
#define POLL_SN_CACHE_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"

// 通道数量
#define CHANNEL_COUNT 4

// SN 长度
#define SN_LENGTH      16

// SN 更新间隔（10分钟）
#define SN_UPDATE_INTERVAL_MS  600000

/**
 * @brief SN 缓存结构
 */
typedef struct {
    bool valid;                    ///< 缓存是否有效
    uint32_t last_update_tick;    ///< 上次更新时间
    uint8_t sn[SN_LENGTH];         ///< SN 数据
    uint8_t sn_len;               ///< SN 长度
} channel_sn_t;

/**
 * @brief 初始化 SN 缓存
 *
 * 清空所有通道的 SN 缓存和命令回复缓存，重置强制更新标志。
 * 通常在任务启动时调用。
 */
void poll_sn_cache_init(void);

/**
 * @brief 强制更新 SN
 *
 * 设置强制更新标志，下次轮询时会忽略 SN 缓存有效期，
 * 强制从设备重新读取 SN。
 */
void poll_sn_cache_force_update(void);

/**
 * @brief 获取指定通道的 SN 缓存
 *
 * @param channel 通道编号
 * @return 指向 SN 缓存结构的指针
 */
const channel_sn_t* poll_sn_cache_get(uint8_t channel);

/**
 * @brief 检查指定通道的 SN 是否需要更新
 *
 * @param channel 通道编号
 * @return true 需要更新，false 可以使用缓存
 */
bool poll_sn_cache_need_update(uint8_t channel);

/**
 * @brief 解析设备回复中的 SN
 *
 * 从 RS485 回复数据中提取 SN 并存入缓存。
 *
 * @param channel 通道编号
 * @param data 回复数据指针
 * @param len 回复数据长度
 * @return true 解析成功，false 解析失败
 */
bool poll_sn_cache_parse(uint8_t channel, const uint8_t *data, int len);

/**
 * @brief 将字节数组转换为十六进制字符串
 *
 * 将二进制数据转换为可读的十六进制格式，字节间用空格分隔。
 * 例如：[0x12, 0x34, 0xAB] -> "12 34 AB"
 *
 * @param data 输入数据指针
 * @param len 输入数据长度
 * @param out 输出字符串缓冲区
 * @param out_len 输出缓冲区大小
 */
void poll_sn_cache_bytes_to_hex(const uint8_t *data, int len, char *out, int out_len);

/**
 * @brief 清除强制更新标志
 *
 * 在 SN 更新完成后调用
 */
void poll_sn_cache_clear_force_update(void);

#endif  // POLL_SN_CACHE_H
