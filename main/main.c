#include "esp_log.h"
#include "wifi_smartconfig.h"
#include "uart_config.h"
#include "mqtt_wrapper.h"
#include "mqtt_cmd.h"
#include "poll_task.h"
#include "sntp_time.h"
#include "common.h"

#define TAG "MAIN"
#define UART_BAUD_RATE 19200


void app_main(void)
{
    ESP_LOGI(TAG, "主程序启动...");
    // WiFi 初始化已连
    wifi_smartconfig_init();
    while (!wifi_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG, "WiFi 已连接");

    // ========== 时间同步 ==========
    sntp_time_init();
    sntp_time_sync_now();

    // 等待首次同步完成（可选）
    while (!sntp_time_is_synced()) {
        ESP_LOGI(TAG, "等待时间同步...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // 获取时间戳
    ESP_LOGI(TAG, "当前时间戳: %lld", sntp_time_get_timestamp());
    // ==============================

    // 串口初始化
    uart_config_init_all(UART_BAUD_RATE);
    ESP_LOGI(TAG, "串口初始化成功");

    // MQTT 初始化
    mqtt_wrapper_init(MQTT_BROKER_URI, MQTT_CLIENT_ID);
    while (!mqtt_wrapper_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG, "MQTT 已连接");

    // 初始化模块
    mqtt_cmd_init();
    poll_task_init();

    ESP_LOGI(TAG, "系统运行中");
}
