idf_component_register(
        SRCS "uart_config.c"
        INCLUDE_DIRS "."
        REQUIRES  driver esp_event rs485 rs232 mux_control
)