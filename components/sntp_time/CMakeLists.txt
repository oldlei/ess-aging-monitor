idf_component_register(
        SRCS "sntp_time.c"
        INCLUDE_DIRS "."
        REQUIRES esp_timer log
)