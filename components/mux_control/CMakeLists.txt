idf_component_register(
        SRCS "mux_control.c"
        INCLUDE_DIRS "."
        REQUIRES driver esp_driver_uart esp_driver_gpio log
)
