#include "mqtt_wrapper.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "string.h"
#include "stdlib.h"
#include <esp_event_base.h>
#include "mqtt_client.h"

static const char *TAG = "MQTT_WRAPPER";
static esp_mqtt_client_handle_t mqtt_client = NULL;
static mqtt_data_callback_t data_callback = NULL;
static bool mqtt_connected = false;

// ==================== MQTT 事件处理 ====================
static void mqtt_event_handler(void* handler_args, esp_event_base_t base,
                                int32_t event_id, void* event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT 已连接");
            mqtt_connected = true;
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT 已断开");
            mqtt_connected = false;
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "收到 MQTT 数据, topic=%.*s, data=%.*s",
                  event->topic_len, event->topic,
                  event->data_len, event->data);

            if (data_callback != NULL && event->data_len > 0) {
                char *topic = malloc(event->topic_len + 1);
                char *data = malloc(event->data_len + 1);
                if (topic != NULL && data != NULL) {
                    memcpy(topic, event->topic, event->topic_len);
                    topic[event->topic_len] = '\0';
                    memcpy(data, event->data, event->data_len);
                    data[event->data_len] = '\0';
                    data_callback(topic, data, event->data_len);
                    free(topic);
                    free(data);
                }
            }
            break;

        default:
            break;
    }
}

// ==================== 初始化 MQTT 客户端 ====================
esp_err_t mqtt_wrapper_init(const char *broker_uri, const char *client_id)
{
    if (mqtt_client != NULL) {
        ESP_LOGW(TAG, "MQTT 客户端已初始化");
        return ESP_OK;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri,
        .credentials = {
            .client_id = client_id,  // 添加这行
        },
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT 客户端初始化失败");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_err_t ret = esp_mqtt_client_start(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MQTT 客户端启动失败");
        return ret;
    }

    ESP_LOGI(TAG, "MQTT 客户端初始化完成");
    return ESP_OK;
}

// ==================== 订阅 MQTT 主题 ====================
esp_err_t mqtt_wrapper_subscribe(const char *topic, int qos)
{
    if (mqtt_client == NULL || !mqtt_connected) {
        ESP_LOGE(TAG, "MQTT 未连接");
        return ESP_FAIL;
    }

    int msg_id = esp_mqtt_client_subscribe(mqtt_client, topic, qos);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "MQTT 订阅失败");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "MQTT 订阅成功, topic=%s", topic);
    return ESP_OK;
}

// ==================== 发布 MQTT 消息 ====================
esp_err_t mqtt_wrapper_publish(const char *topic, const char *data, size_t len)
{
    if (mqtt_client == NULL || !mqtt_connected) {
        ESP_LOGE(TAG, "MQTT 未连接");
        return ESP_FAIL;
    }

    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, data, len, 1, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "MQTT 发布失败");
        return ESP_FAIL;
    }

    return ESP_OK;
}

// ==================== 注册回调函数 ====================
void mqtt_wrapper_set_callback(mqtt_data_callback_t callback)
{
    data_callback = callback;
}

// ==================== 检查连接状态 ====================
bool mqtt_wrapper_is_connected(void)
{
    return mqtt_connected;
}
