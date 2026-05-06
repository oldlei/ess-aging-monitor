//
// Created by 成雷 on 2026/5/5.
//

#include "sntp_time.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "lwip/apps/sntp.h"
#include "time.h"
#include "sys/time.h"

#define TAG "SNTP_TIME"

// ==================== 配置 ====================
#define SNTP_SERVER_1       "ntp.aliyun.com"
#define SNTP_SERVER_2       "ntp.tencent.com"
#define SNTP_SERVER_3       "ntp.huawei.com"
#define SNTP_SERVER_4       "cn.ntp.org.cn"
#define SNTP_SERVER_5       "pool.ntp.org"
#define SYNC_INTERVAL_MS    (0.5 * 60 * 1000)   // 5分钟
#define TIMEZONE_OFFSET     8                 // 东八区

// ==================== 静态变量 ====================
static bool g_time_synced = false;
static esp_timer_handle_t g_check_timer_handle = NULL;  // 检查同步状态
static esp_timer_handle_t g_periodic_timer_handle = NULL;  // 周期同步
static bool g_initialized = false;

// ==================== 内部函数 ====================

// 设置系统时区
static void set_timezone(void)
{
    setenv("TZ", "UTC-8", 1);
    tzset();
    ESP_LOGI(TAG, "时区已设置为 UTC+%d", TIMEZONE_OFFSET);
}

// 打印当前时间
static void print_current_time(void)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    ESP_LOGI(TAG, "==================================");
    ESP_LOGI(TAG, "  时间同步成功！");
    ESP_LOGI(TAG, "  UTC 时间戳: %lld", (int64_t)now);
    ESP_LOGI(TAG, "  本地时间:   %04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec);
    ESP_LOGI(TAG, "==================================");
}

// 检查时间是否有效（2010年之后）
static bool is_time_valid(void)
{
    time_t now;
    time(&now);
    return (now > 1262304000);  // 2010-01-01 00:00:00 UTC
}


// 定时检查同步状态
static void check_sync_timer_callback(void* arg)
{
    (void)arg;

    if (!g_time_synced && is_time_valid()) {
        g_time_synced = true;
        print_current_time();
        ESP_LOGI(TAG, "周期同步已启动，间隔: %d ms (%d 分钟)",
                 SYNC_INTERVAL_MS, SYNC_INTERVAL_MS / 60000);
    }
}

// 定时周期同步（NTP 本身会处理，这里仅做日志记录）
static void periodic_sync_callback(void* arg)
{
    (void)arg;

    ESP_LOGI(TAG, "执行周期时间同步...");
    sntp_stop();
    sntp_init();
}

// ==================== 对外接口 ====================

esp_err_t sntp_time_init(void)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "SNTP 已初始化");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "==================================");
    ESP_LOGI(TAG, "   SNTP 时间同步初始化");
    ESP_LOGI(TAG, "==================================");

    // 设置时区
    set_timezone();

    // 配置 NTP 服务器
    sntp_setservername(0, SNTP_SERVER_1);
    sntp_setservername(1, SNTP_SERVER_2);
    sntp_setservername(2, SNTP_SERVER_3);
    sntp_setservername(3, SNTP_SERVER_4);
    sntp_setservername(4, SNTP_SERVER_5);

    ESP_LOGI(TAG, "  主服务器: %s", sntp_getservername(0));
    ESP_LOGI(TAG, "  备服务器: %s", sntp_getservername(1));
    ESP_LOGI(TAG, "  备服务器: %s", sntp_getservername(2));
    ESP_LOGI(TAG, "  备服务器: %s", sntp_getservername(3));
    ESP_LOGI(TAG, "  备服务器: %s", sntp_getservername(4));

    // 初始化 SNTP（同步模式：收到响应后立即更新时间）
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_init();

    g_initialized = true;

    ESP_LOGI(TAG, "SNTP 初始化完成");
    ESP_LOGI(TAG, "  主服务器: %s", SNTP_SERVER_1);
    ESP_LOGI(TAG, "  备服务器: %s", SNTP_SERVER_2);

    return ESP_OK;
}

esp_err_t sntp_time_sync_now(void)
{
    if (!g_initialized) {
        ESP_LOGE(TAG, "请先调用 sntp_time_init()");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "立即触发时间同步...");

    // 重新初始化触发同步
    sntp_stop();
    sntp_init();

    // 创建检查定时器（每秒检查是否同步成功）
    if (g_check_timer_handle == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = &check_sync_timer_callback,
            .arg = NULL,
            .name = "sntp_check_sync",
            .skip_unhandled_events = true,
        };

        esp_err_t ret = esp_timer_create(&timer_args, &g_check_timer_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "创建检查定时器失败: %s", esp_err_to_name(ret));
            return ret;
        }

        // 启动检查定时器
        esp_timer_start_periodic(g_check_timer_handle, 1000000);  // 1秒
    }

    // 创建周期同步定时器（每5分钟重新同步一次）
    if (g_periodic_timer_handle == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = &periodic_sync_callback,
            .arg = NULL,
            .name = "sntp_periodic_sync",
            .skip_unhandled_events = true,
        };

        esp_err_t ret = esp_timer_create(&timer_args, &g_periodic_timer_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "创建周期同步定时器失败: %s", esp_err_to_name(ret));
            return ret;
        }

        // 启动周期同步定时器
        esp_timer_start_periodic(g_periodic_timer_handle, SYNC_INTERVAL_MS * 1000);  // 微秒
        ESP_LOGI(TAG, "周期同步定时器已启动，间隔: %d ms (%d 分钟)",
                 SYNC_INTERVAL_MS, SYNC_INTERVAL_MS / 60000);
    }

    return ESP_OK;
}

bool sntp_time_is_synced(void)
{
    return g_time_synced;
}

int64_t sntp_time_get_timestamp(void)
{
    if (!g_time_synced && !is_time_valid()) {
        return -1;
    }

    time_t now;
    time(&now);
    return (int64_t)now;
}

void sntp_time_deinit(void)
{
    if (g_check_timer_handle != NULL) {
        esp_timer_stop(g_check_timer_handle);
        esp_timer_delete(g_check_timer_handle);
        g_check_timer_handle = NULL;
    }

    if (g_periodic_timer_handle != NULL) {
        esp_timer_stop(g_periodic_timer_handle);
        esp_timer_delete(g_periodic_timer_handle);
        g_periodic_timer_handle = NULL;
        ESP_LOGI(TAG, "周期同步定时器已停止");
    }

    sntp_stop();

    g_time_synced = false;
    g_initialized = false;

    ESP_LOGI(TAG, "SNTP 已反初始化");
}
