//
// Created by 成雷 on 2026/4/29.
//

/**
 * @file poll_task.c
 * @brief 统一轮询任务实现
 *
 * 该模块负责通过 UART (RS485/RS232) 与 ESS 设备通信，轮询各个通道的状态数据，
 * 并将结果通过 MQTT 协议发布到指定主题。支持 RS485 和 RS232 两种通信模式。
 */

#include "poll_task.h"

#include <sntp_time.h>

#include "poll_cmd.h"
#include "poll_sn_cache.h"
#include "poll_uart_safe.h"
#include "mqtt_cmd.h"
#include "esp_log.h"
#include "mux_control.h"
#include "uart_config.h"
#include "mqtt_wrapper.h"
#include <string.h>
#include <stdio.h>

// 日志标签
#define TAG "POLL_TASK"

// 等待回复超时时间（毫秒）
#define RESPONSE_TIMEOUT_MS 500

// MQTT 主题定义
#define MQTT_TOPIC_RS485 "uart/rs485"  ///< RS485 模式 MQTT 主题
#define MQTT_TOPIC_RS232 "uart/rs232"   ///< RS232 模式 MQTT 主题

// 定时发送控制（供外部访问）
volatile bool timer_send_enabled = false;
uint32_t timer_send_interval = 1000;

// 回复缓存（供外部访问）
static char g_cmd_response[CHANNEL_COUNT][POLL_CMD_COUNT][323];

// ==================== 统一轮询任务 ====================
/**
 * @brief 统一轮询任务主函数
 *
 * 这是一个 FreeRTOS 任务，负责：
 * 1. 根据当前 UART 模式（RS485/RS232）选择合适的通信接口
 * 2. 通过 MUX 切换到下一个通道
 * 3. 发送轮询命令并接收回复
 * 4. 解析 SN（仅 RS485 模式）
 * 5. 将结果封装为 JSON 并通过 MQTT 发布
 *
 * @param pvParameters 任务参数（未使用）
 */
static void poll_task(void *pvParameters)
{
    uint8_t rx_buffer[128];  ///< 接收缓冲区（512 字节）
    uint8_t tx_buffer[16];   ///< 发送缓冲区

    poll_sn_cache_init();

    while (1) {
        // 检查是否启用定时发送
        if (timer_send_enabled) {
            // 根据当前 UART 模式选择 MUX 类型和 MQTT 主题
            mux_type_t mux_type = (current_uart_mode == UART_MODE_RS485) ? MUX_TYPE_RS485 : MUX_TYPE_RS232;
            const char *mqtt_topic = (current_uart_mode == UART_MODE_RS485) ? MQTT_TOPIC_RS485 : MQTT_TOPIC_RS232;
            const char *mode_name = (current_uart_mode == UART_MODE_RS485) ? "RS485" : "RS232";

            // 获取下一个要轮询的通道
            mux_channel_t ch = mux_next_channel(mux_type);
            ESP_LOGI(TAG, "%s 轮询通道 %d", mode_name, ch);

            // 清空该通道的回复缓存
            memset(g_cmd_response[ch], 0, sizeof(g_cmd_response[ch]));

            // RS485 模式：检查是否需要更新 SN
            // RS232 模式：不需要 SN
            bool do_sn_update = (current_uart_mode == UART_MODE_RS485) && poll_sn_cache_need_update(ch);

            // 遍历所有轮询命令
            for (int cmd_idx = 0; cmd_idx < POLL_CMD_COUNT; cmd_idx++) {
                // RS485 poll_cmd_1：检查 SN 缓存
                if (current_uart_mode == UART_MODE_RS485 && cmd_idx == 0) {
                    if (!do_sn_update) {
                        ESP_LOGI(TAG, "RS485 CH%d 跳过 poll_cmd_1 (SN有效)", ch);
                        continue;
                    }
                }

                // 准备发送数据
                memcpy(tx_buffer, poll_commands[cmd_idx], poll_cmd_lengths[cmd_idx]);
                int tx_len = poll_cmd_lengths[cmd_idx];

                // RS485 需要附加通道号（用于目标设备识别）
                if (current_uart_mode == UART_MODE_RS485) {
                    tx_buffer[tx_len] = (uint8_t)ch;
                    tx_len++;
                }

                // 发送命令并接收回复
                int sent_len;
                int recv_len;
                if (current_uart_mode == UART_MODE_RS485) {
                    sent_len = rs485_write_safe(tx_buffer, tx_len);
                    if (sent_len > 0) {
                        ESP_LOGI(TAG, "RS485 CH%d 发送命令%d", ch, cmd_idx + 1);
                        recv_len = rs485_read_safe(rx_buffer, sizeof(rx_buffer), RESPONSE_TIMEOUT_MS);
                    } else {
                        recv_len = -1;
                    }
                } else {
                    sent_len = rs232_write_safe(tx_buffer, tx_len);
                    if (sent_len > 0) {
                        ESP_LOGI(TAG, "RS232 CH%d 发送命令%d", ch, cmd_idx + 1);
                        recv_len = rs232_read_safe(rx_buffer, sizeof(rx_buffer), RESPONSE_TIMEOUT_MS);
                    } else {
                        recv_len = -1;
                    }
                }

                // 处理发送结果
                if (sent_len > 0) {
                    if (recv_len > 0) {
                        // 收到有效回复：转换为十六进制字符串并保存
                        poll_sn_cache_bytes_to_hex(rx_buffer, recv_len, g_cmd_response[ch][cmd_idx], 323);

                        ESP_LOGI(TAG, "%s CH%d 命令%d 收到 [%d]: %s",
                                  mode_name, ch, cmd_idx + 1, recv_len, g_cmd_response[ch][cmd_idx]);

                        // RS485：解析 SN（poll_cmd_1 回复）
                        if (current_uart_mode == UART_MODE_RS485 && cmd_idx == 0) {
                            if (poll_sn_cache_parse(ch, rx_buffer, recv_len)) {
                                ESP_LOGI(TAG, "RS485 CH%d SN已更新", ch);
                            }
                        }
                    } else {
                        snprintf(g_cmd_response[ch][cmd_idx], 256, "TIMEOUT");
                        ESP_LOGW(TAG, "%s CH%d 命令%d 超时", mode_name, ch, cmd_idx + 1);
                    }
                } else {
                    snprintf(g_cmd_response[ch][cmd_idx], 256, "SEND_FAIL");
                    ESP_LOGW(TAG, "%s CH%d 命令%d 发送失败", mode_name, ch, cmd_idx + 1);
                }
            }

            // RS485 模式：清除强制更新标志
            if (do_sn_update) {
                poll_sn_cache_clear_force_update();
            }

            // 构造 MQTT 消息并发布
            char mqtt_payload[2200];
            if (current_uart_mode == UART_MODE_RS485) {
                // RS485 模式：包含 SN 字段
                const channel_sn_t *sn = poll_sn_cache_get(ch);
                char sn_temp_str[31] = "";// SN序列号
                ESP_LOGI("POLL_TASK", "SN valid:%d, len:%d, sn:%s, check:0x%04X",sn->valid,sn->sn_len,sn_temp_str,sn->sn_check);

                // todo  判断存在的必要性？是否可删除 poll_sn_cache_bytes_to_string   这里将SN转为字符串输出，改到在收到数据后直接转为字符串
                if (sn->valid) {
                    poll_sn_cache_bytes_to_string(sn->sn,30,sn_temp_str,32);
                }
                int64_t timestamp = sntp_time_get_timestamp();
                uint16_t timestamp_check = crc16_modbus((const uint8_t*)timestamp, sizeof(timestamp));
                // TODO 检查能否收到
                snprintf(mqtt_payload, sizeof(mqtt_payload),
                         "{\"ch\":%d,\"SN\":\"%s\",\"sn_check\":\"%04X\",\"time\":%llu,\"timestamp_check\":\"%04X\",\"PIA\":\"%s\",\"PIB\":\"%s\",\"PIC\":\"%s\"}",
                         ch, sn->sn,sn->sn_check,
                         timestamp,timestamp_check, g_cmd_response[ch][1],
                         g_cmd_response[ch][2], g_cmd_response[ch][3]);
                printf("\r\n");
                printf("SN:%s,SN_CHECK:%04x", sn->sn, sn->sn_check);
                printf("\r\n");
            } else {
                // RS232 模式：无 SN 字段
                snprintf(mqtt_payload, sizeof(mqtt_payload),
                         "{\"ch\":%d,\"cmd1\":\"%s\",\"cmd2\":\"%s\",\"cmd3\":\"%s\",\"cmd4\":\"%s\"}",
                         ch,
                         g_cmd_response[ch][0], g_cmd_response[ch][1],
                         g_cmd_response[ch][2], g_cmd_response[ch][3]);
            }

            // 发布 MQTT 消息
            mqtt_wrapper_publish(mqtt_topic, mqtt_payload, strlen(mqtt_payload));
            ESP_LOGI(TAG, "%s CH%d MQTT已发布", mode_name, ch);

            // 等待下一个轮询周期
            ESP_LOGI(TAG, "%s CH%d 完成，等待 %lums", mode_name, ch, timer_send_interval);
            vTaskDelay(pdMS_TO_TICKS(timer_send_interval));
        } else {
            // 未启用发送时，低频率检查
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

// ==================== 初始化函数 ====================
void poll_task_init(void)
{
    xTaskCreate(poll_task, "uart_poll", 8192, NULL, 5, NULL);
    ESP_LOGI(TAG, "统一轮询任务已启动");
}

void poll_task_force_update_sn(void)
{
    poll_sn_cache_force_update();
}

const response_array_t* poll_task_get_responses(uint8_t channel)
{
    return (const response_array_t*)g_cmd_response[channel];
}
