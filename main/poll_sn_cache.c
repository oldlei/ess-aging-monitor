//
// Created by 成雷 on 2026/4/29.
//

/**
 * @file poll_sn_cache.c
 * @brief SN 缓存管理实现
 */

#include "poll_sn_cache.h"
#include "esp_log.h"
#include "poll_cmd.h"
#include <string.h>
#include <stdio.h>

// 回复数据缓存
static char cmd_response[CHANNEL_COUNT][POLL_CMD_COUNT][400];

// 强制更新标志
static bool force_sn_update = false;

// SN 缓存
static channel_sn_t sn_cache[CHANNEL_COUNT] = {0};

void poll_sn_cache_init(void)
{
    memset(sn_cache, 0, sizeof(sn_cache));
    memset(cmd_response, 0, sizeof(cmd_response));
    force_sn_update = false;
    ESP_LOGI("SN_CACHE", "SN 缓存已初始化");
}

void poll_sn_cache_force_update(void)
{
    force_sn_update = true;
    ESP_LOGI("SN_CACHE", "强制更新 SN 标志已设置");
}

const channel_sn_t* poll_sn_cache_get(uint8_t channel)
{
    return &sn_cache[channel];
}

bool poll_sn_cache_need_update(uint8_t channel)
{
    if (force_sn_update) return true;
    if (!sn_cache[channel].valid) return true;

    uint32_t elapsed = (xTaskGetTickCount() - sn_cache[channel].last_update_tick)
                       * portTICK_PERIOD_MS;
    return (elapsed >= SN_UPDATE_INTERVAL_MS);
}

uint16_t crc16_modbus(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;   // 0xA001 = 0x8005 反转
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

bool verify_frame_crc(const uint8_t *frame, size_t len) {
    if (len < 2) return false;
    size_t data_len = len - 2;
    uint16_t calc_crc = crc16_modbus(frame, data_len);
    // 提取帧中存储的CRC（低字节在前）
    uint16_t recv_crc = (uint16_t)frame[data_len] | ((uint16_t)frame[data_len + 1] << 8);
    return calc_crc == recv_crc;
}

bool poll_sn_cache_parse(uint8_t channel, const uint8_t *data, int len)
{
    if (!verify_frame_crc(data,len)) return false;
    memcpy(sn_cache[channel].sn, &data[45], 30);
    sn_cache[channel].sn_check = crc16_modbus(&data[45],30);
    sn_cache[channel].sn_len = 30;
    sn_cache[channel].valid = true;
    sn_cache[channel].last_update_tick = xTaskGetTickCount();
    return true;
}

/**
 * @brief 将二进制数据转换为空格分隔的十六进制字符串（如：01 02 AB CD）
 * @param data  原始二进制数据
 * @param len   数据长度
 * @param out   输出字符串缓冲区
 * @param out_len 输出缓冲区最大长度（包含结束符）
 * @note 输出格式：每个字节占 2 个十六进制字符 + 1 个空格，最后一个字节无空格
 */
// void poll_sn_cache_bytes_to_hex(const uint8_t *data, int len, char *out, int out_len)
// {
//     if (data == NULL || out == NULL || out_len < 2) {
//         return;
//     }
//
//     int pos = 0;
//     for (int i = 0; i < len && pos < out_len - 1; i++) {
//         pos += snprintf(out + pos, out_len - pos, "%02X", data[i]);
//         if (i < len - 1) out[pos++] = ' ';
//     }
// }
void poll_sn_cache_bytes_to_hex(const uint8_t *data, int len, char *out, int out_len)
{
    if (data == NULL || out == NULL || out_len < 2) {
        return;
    }

    int pos = 0;
    out[0] = '\0';

    // 核心修复：只保证不越界，不提前截断
    for (int i = 0; i < len; i++) {
        // 剩余不足 3 字节（XX+空格），且不是最后一个字节 → 停止
        if (pos + 3 > out_len) {
            // 但！如果是最后一个字节，只需要 2 字节，依然可以写
            if (i == len - 1 && pos + 2 <= out_len) {
                // 写最后一个字节（无空格）
                snprintf(out + pos, out_len - pos, "%02X", data[i]);
                // =====
                printf("%02X", data[i]);
            }
            break;
        }

        // 写入十六进制
        pos += snprintf(out + pos, out_len - pos, "%02X", data[i]);

        // 不是最后一个字节才加空格
        if (i < len - 1) {
            out[pos++] = ' ';
        }
    }

    // 确保字符串结束
    out[pos] = '\0';
}

void poll_sn_cache_clear_force_update(void)
{
    force_sn_update = false;
}

/**
 * @brief 获取命令回复缓存指针
 *
 * @param channel 通道编号
 * @param cmd_idx 命令索引
 * @return 回复字符串指针
 */
const char* poll_sn_cache_get_response(uint8_t channel, uint8_t cmd_idx)
{
    return cmd_response[channel][cmd_idx];
}

/**
 * @brief 写入命令回复到缓存
 *
 * @param channel 通道编号
 * @param cmd_idx 命令索引
 * @param response 回复字符串
 */
void poll_sn_cache_set_response(uint8_t channel, uint8_t cmd_idx, const char *response)
{
    strncpy(cmd_response[channel][cmd_idx], response, 255);
    cmd_response[channel][cmd_idx][255] = '\0';
}

/**
 * @brief 清除指定通道的回复缓存
 *
 * @param channel 通道编号
 */
void poll_sn_cache_clear_response(uint8_t channel)
{
    memset(cmd_response[channel], 0, sizeof(cmd_response[channel]));
}

/**
 * @brief 获取所有通道的回复缓存
 *
 * @param channel 通道编号
 * @return 回复缓存数组指针
 */
const char (*poll_sn_cache_get_all_responses(uint8_t channel))[POLL_CMD_COUNT][400]
{
    return &cmd_response[channel];
}


void poll_sn_cache_bytes_to_string(const uint8_t *bytes, int len, char *out, int out_len)
{
    // 清空缓冲区（防止残留）
    memset(out, 0, out_len);

    int offset = 0;
    for (int i = 0; i < len && offset < out_len - 2; i++) {
        // ✅ snprintf 限制长度，绝对安全
        offset += snprintf(out + offset, out_len - offset, "%02x", bytes[i]);
    }

    // 强制结束符
    out[out_len - 1] = '\0';
}