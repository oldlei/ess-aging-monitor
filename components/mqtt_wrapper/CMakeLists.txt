idf_component_register(
        SRCS "mqtt_wrapper.c"
        INCLUDE_DIRS "."
        REQUIRES  driver esp_event espressif__mqtt
)