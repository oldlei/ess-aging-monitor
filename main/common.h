//
// Created by 成雷 on 2026/5/7.
//

#ifndef ESS_AGING_MONITOR_COMMON_H
#define ESS_AGING_MONITOR_COMMON_H

#define MQTT_BROKER_URI "mqtt://192.168.4.6"
#define MQTT_CLIENT_ID "id001"

// MQTT 主题定义
// #define MQTT_TOPIC_RS485 "uart/rs485"  ///< RS485 模式 MQTT 主题
// #define MQTT_FULL_TOPIC(client, topic) client "/" topic
#define MQTT_TOPIC_RS485(client) "uart/rs485/" client //< RS485 模式 MQTT 主题

#define MQTT_TOPIC_RS232 "uart/rs232"   ///< RS232 模式 MQTT 主题


#endif //ESS_AGING_MONITOR_COMMON_H