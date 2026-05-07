#ifndef MQTT_WRAPPER_H
#define MQTT_WRAPPER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * @brief  MQTT 数据回调函数类型
     * @param  topic 接收数据的主题
     * @param  data 接收到的数据
     * @param  len 数据长度
     */
    typedef void (*mqtt_data_callback_t)(const char *topic, const char *data, size_t len);

    /**
     * @brief  初始化 MQTT 客户端
     * @param  broker_uri MQTT 代理地址，如 "mqtt://mqtt.eclipseprojects.io"
     * @param  client_id MQTT ID
     * @return esp_err_t
     */
    esp_err_t mqtt_wrapper_init(const char *broker_uri, const char *client_id);

    /**
     * @brief  订阅 MQTT 主题
     * @param  topic 主题名称
     * @param  qos 服务质量等级 (0, 1, 2)
     * @return esp_err_t
     */
    esp_err_t mqtt_wrapper_subscribe(const char *topic, int qos);

    /**
     * @brief  发布 MQTT 消息
     * @param  topic 主题名称
     * @param  data 数据内容
     * @param  len 数据长度
     * @return esp_err_t
     */
    esp_err_t mqtt_wrapper_publish(const char *topic, const char *data, size_t len);

    /**
     * @brief  注册数据接收回调
     * @param  callback 回调函数
     */
    void mqtt_wrapper_set_callback(mqtt_data_callback_t callback);

    /**
     * @brief  检查 MQTT 连接状态
     * @return true=已连接 / false=未连接
     */
    bool mqtt_wrapper_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif // MQTT_WRAPPER_H
