//
// Created by 成雷 on 2026/5/5.
//

#ifndef ESS_AGING_MONITOR_SNTP_TIME_H
#define ESS_AGING_MONITOR_SNTP_TIME_H

#include "esp_err.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * @brief  初始化 SNTP 时间同步组件
     * @note   需在 WiFi 连接后调用
     * @return ESP_OK=成功
     */
    esp_err_t sntp_time_init(void);

    /**
     * @brief  立即同步一次时间
     * @note   开机联网后调用一次，后续由定时器自动同步
     * @return ESP_OK=成功
     */
    esp_err_t sntp_time_sync_now(void);

    /**
     * @brief  获取时间是否已同步
     * @return true=已同步 / false=未同步
     */
    bool sntp_time_is_synced(void);

    /**
     * @brief  获取当前 Unix 时间戳（秒）
     * @return 时间戳，-1=未同步
     */
    int64_t sntp_time_get_timestamp(void);

    /**
     * @brief  停止定时同步（释放资源）
     * @note   通常不需要调用
     */
    void sntp_time_deinit(void);

#ifdef __cplusplus
}
#endif

#endif //ESS_AGING_MONITOR_SNTP_TIME_H
