#include "wifi_smartconfig.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_smartconfig.h"
#include "esp_netif.h"
#include "rgb_led.h"

// ==================== 配置 ====================
#define TAG                 "WIFI_SC"
#define WIFI_CONNECT_RETRY  5

// ==================== 全局静态变量 ====================
static bool wifi_connected = false;
static uint8_t connect_retry_cnt = 0;
static bool smartconfig_running = false;    // 修复问题2: 跟踪 SmartConfig 状态

// ==================== 呼吸灯函数 ====================
static void breathe_led(rgb_color_t color)
{
    for (int i = 1; i <= 50; i++) {
        rgb_led_set_brightness(i);
        rgb_led_set_all(color);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    for (int i = 50; i >= 1; i--) {
        rgb_led_set_brightness(i);
        rgb_led_set_all(color);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    rgb_led_clear();
    vTaskDelay(pdMS_TO_TICKS(20));
}

// ==================== LED 状态任务 ====================
static void led_task(void *arg)
{
    while (1) {
        if (wifi_connected) {
            breathe_led(RGB_GREEN);
        } else {
            breathe_led(RGB_RED);
        }
    }
}

// ==================== 检查 NVS 中是否有保存的 WiFi 凭据 ====================
// 修复问题3: 无凭据时直接跳过空连接
static bool has_saved_wifi_config(void)
{
    wifi_config_t cfg = {0};
    if (esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK) {
        return cfg.sta.ssid[0] != '\0';
    }
    return false;
}

// ==================== WiFi 事件处理 ====================
static void wifi_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA 启动，尝试连接...");
        esp_wifi_connect();
    }
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "WiFi 已连接，等待IP...");
    }
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;

        // 修复问题2: SmartConfig 正在运行时，不处理断连事件
        if (smartconfig_running) {
            ESP_LOGW(TAG, "SmartConfig 运行中，忽略断连事件");
            return;
        }

        if (connect_retry_cnt < WIFI_CONNECT_RETRY) {
            ESP_LOGW(TAG, "断开，自动重连 (%d/%d)", ++connect_retry_cnt, WIFI_CONNECT_RETRY);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "重连失败 → 进入 SmartConfig 配网");
            smartconfig_running = true;
            static const smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
            esp_smartconfig_start(&cfg);
        }
    }
}

// ==================== IP 事件处理 ====================
static void ip_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    ip_event_got_ip_t *evt = (ip_event_got_ip_t *)data;
    ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&evt->ip_info.ip));
    wifi_connected = true;
    connect_retry_cnt = 0;
    smartconfig_running = false;
    esp_smartconfig_stop();
}

// ==================== SmartConfig 事件处理 ====================
static void sc_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    if (id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "SmartConfig 扫描完成");
    }
    else if (id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG, "SmartConfig 找到目标通道");
    }
    else if (id == SC_EVENT_GOT_SSID_PSWD) {
        ESP_LOGI(TAG, "配网成功，获取WiFi信息");
        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)data;

        wifi_config_t cfg = {0};
        memcpy(cfg.sta.ssid, evt->ssid, sizeof(cfg.sta.ssid));
        memcpy(cfg.sta.password, evt->password, sizeof(cfg.sta.password));

        ESP_LOGI(TAG, "SSID: %s", (char *)cfg.sta.ssid);

        // 修复问题1: 重置重试计数器，让新凭据有机会重试
        connect_retry_cnt = 0;
        smartconfig_running = false;

        esp_wifi_set_config(WIFI_IF_STA, &cfg);
        esp_smartconfig_stop();
        esp_wifi_connect();
    }
}

// ==================== WiFi 初始化 ====================
static void wifi_init(void)
{
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_handler, NULL);
    esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, sc_event_handler, NULL);

    esp_wifi_start();

    // 修复问题3: 无保存凭据时直接进入 SmartConfig，跳过5次空连接
    if (!has_saved_wifi_config()) {
        ESP_LOGI(TAG, "无保存的WiFi凭据，直接进入 SmartConfig 配网");
        smartconfig_running = true;
        static const smartconfig_start_config_t sc_cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
        esp_smartconfig_start(&sc_cfg);
    }
}

// ==================== NVS 初始化 ====================
static esp_err_t nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    return ret;
}

// ==================== 对外接口：初始化 ====================
esp_err_t wifi_smartconfig_init(void)
{
    esp_err_t ret = nvs_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS 初始化失败");
        return ret;
    }

    esp_netif_init();
    esp_event_loop_create_default();

    rgb_led_init();
    rgb_led_set_brightness(10);
    xTaskCreate(led_task, "led_task", 2048, NULL, 1, NULL);

    wifi_init();

    ESP_LOGI(TAG, "==================================");
    ESP_LOGI(TAG, "   ESP32 SmartConfig 驱动初始化完成");
    ESP_LOGI(TAG, "==================================");

    return ESP_OK;
}

// ==================== 对外接口：获取连接状态 ====================
bool wifi_is_connected(void)
{
    return wifi_connected;
}

// ==================== 对外接口：重新配网（可选） ====================
esp_err_t wifi_smartconfig_restart(void)
{
    ESP_LOGI(TAG, "手动触发重新配网");
    connect_retry_cnt = 0;
    wifi_connected = false;
    smartconfig_running = true;

    esp_smartconfig_stop();
    esp_wifi_disconnect();

    static const smartconfig_start_config_t sc_cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    return esp_smartconfig_start(&sc_cfg);
}
