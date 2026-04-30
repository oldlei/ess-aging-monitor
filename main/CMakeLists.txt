idf_component_register(SRCS "main.c" "mqtt_cmd.c" "poll_task.c"
                    INCLUDE_DIRS "."
        REQUIRES nvs_flash esp_wifi esp_event esp_netif esp_driver_gpio esp_driver_rmt driver hal wifi_smartconfig rgb_led rs485 mqtt_wrapper espressif__mqtt uart_config mux_control
)
