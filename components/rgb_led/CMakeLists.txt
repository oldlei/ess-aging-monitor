idf_component_register(
        SRCS "rgb_led.c"
        INCLUDE_DIRS "."
        REQUIRES esp_driver_rmt esp_driver_gpio driver led_strip # 这一行是关键！子组件必须显式依赖 driver
)