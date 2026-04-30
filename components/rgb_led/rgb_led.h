//
// Created by 成雷 on 2026/3/27.
//

#ifndef DEMO_RGB_LED_H
#define DEMO_RGB_LED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RGB_LED_GPIO_NUM     GPIO_NUM_48
#define RGB_LED_COUNT        1

    typedef struct {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    } rgb_color_t;

    extern const rgb_color_t RGB_RED;
    extern const rgb_color_t RGB_GREEN;
    extern const rgb_color_t RGB_BLUE;
    extern const rgb_color_t RGB_YELLOW;
    extern const rgb_color_t RGB_CYAN;
    extern const rgb_color_t RGB_PURPLE;
    extern const rgb_color_t RGB_WHITE;
    extern const rgb_color_t RGB_BLACK;

    void rgb_led_init(void);
    void rgb_led_set_brightness(uint8_t brightness);
    void rgb_led_set_color(uint8_t index, rgb_color_t color);
    void rgb_led_set_all(rgb_color_t color);
    void rgb_led_clear(void);

#ifdef __cplusplus
}
#endif

#endif //DEMO_RGB_LED_H