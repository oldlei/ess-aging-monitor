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

bool poll_sn_cache_parse(uint8_t channel, const uint8_t *data, int len)
{
    // 假设 SN 在 data[8] ~ data[15]，共 8 字节
    // TODO: 根据实际协议修改此处
    if (len < 16) return false;

    memcpy(sn_cache[channel].sn, &data[8], 8);
    sn_cache[channel].sn_len = 8;
    sn_cache[channel].valid = true;
    sn_cache[channel].last_update_tick = xTaskGetTickCount();

    return true;
}

void poll_sn_cache_bytes_to_hex(const uint8_t *data, int len, char *out, int out_len)
{
    int pos = 0;
    for (int i = 0; i < len && pos < out_len - 3; i++) {
        pos += snprintf(out + pos, out_len - pos, "%02X", data[i]);
        if (i < len - 1) out[pos++] = ' ';
    }
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
