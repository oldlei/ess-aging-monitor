idf_component_register(
        SRCS "wifi_smartconfig.c"
        INCLUDE_DIRS "."
        REQUIRES nvs_flash esp_wifi esp_event esp_netif esp_driver_gpio esp_driver_rmt driver hal wifi_smartconfig rgb_led  # 这一行是关键！子组件必须显式依赖 driver
)