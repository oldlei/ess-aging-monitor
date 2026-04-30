//
// Created by 成雷 on 2026/3/27.
//

#ifndef DEMO_WIFI_SMARTCONFIG_H
#define DEMO_WIFI_SMARTCONFIG_H


#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * @brief  初始化 WiFi + SmartConfig 配网驱动（含自动重连+配网失败进入配网模式）
     * @return 错误码
     */
    esp_err_t wifi_smartconfig_init(void);

    /**
     * @brief  获取当前 WiFi 连接状态
     * @return true=已连接 / false=未连接
     */
    bool wifi_is_connected(void);

#ifdef __cplusplus
}
#endif


#endif //DEMO_WIFI_SMARTCONFIG_H