idf_component_register(
        SRCS "rs485.c"
        INCLUDE_DIRS "."
        REQUIRES driver esp_driver_uart esp_driver_gpio log # 这一行是关键！子组件必须显式依赖 driver
)