//
// Created by 成雷 on 2026/3/27.
//
#include "rgb_led.h"
#include "freertos/FreeRTOS.h"
#include "led_strip.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"

/**
 * @file rgb_led.c
 * @brief RGB LED 控制模块
 * @details 使用官方 led_strip 库控制 WS2812 LED
 */


/**
 * @brief 常用颜色定义
 * @details 预定义的 RGB 颜色值，方便直接使用
 */
const rgb_color_t RGB_RED    = {255, 0, 0};    // 红色
const rgb_color_t RGB_GREEN  = {0, 255, 0};    // 绿色
const rgb_color_t RGB_BLUE   = {0, 0, 255};    // 蓝色
const rgb_color_t RGB_YELLOW = {255, 255, 0};  // 黄色
const rgb_color_t RGB_CYAN   = {0, 255, 255};  // 青色
const rgb_color_t RGB_PURPLE = {255, 0, 255};  // 紫色
const rgb_color_t RGB_WHITE  = {255, 255, 255};// 白色
const rgb_color_t RGB_BLACK  = {0, 0, 0};      // 黑色（关闭）
/**
 * @brief LED strip 句柄
 * @details 用于操作 LED strip 的句柄，由 led_strip_new_rmt_device 创建
 */
static led_strip_handle_t g_led_strip = NULL;
/**
 * @brief 亮度值
 * @details 范围 0-255，默认值为 1（低亮度）
 */
static uint8_t g_brightness = 1;
/**
 * @brief 日志标签
 * @details 用于 ESP_LOG 系列函数的标签
 */
static const char *TAG = "rgb_led";

/**
 * @brief 初始化 RGB LED
 * @details 使用官方 led_strip 库初始化 WS2812 LED
 * @return 无
 */
void rgb_led_init(void)
{

    ESP_LOGI(TAG, "初始化 LED Strip...");

    // LED 条配置
    led_strip_config_t strip_config = {
        .strip_gpio_num = RGB_LED_GPIO_NUM,// LED 数据引脚
        .max_leds = RGB_LED_COUNT,// LED 数量
        .led_model = LED_MODEL_WS2812,// LED 型号
    };

    // RMT 驱动配置
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,// 默认时钟源
        .resolution_hz = 10 * 1000 * 1000, // 10MHz 时钟分辨率，根据 WS2812 规范设置
    };

    // 创建 LED 驱动
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &g_led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建 LED Strip 失败: %d", ret);
        return;
    }
    // 初始灭灯
    led_strip_clear(g_led_strip);
    ESP_LOGI(TAG, "LED Strip 初始化完成");
}

/**
 * @brief 设置 LED 亮度
 * @details 范围 0-255，默认值为 1（低亮度）
 * @param brightness 亮度值
 * @return 无
 */
void rgb_led_set_brightness(uint8_t brightness)
{
    g_brightness = brightness;
}

/**
 * @brief 设置 LED 颜色
 * @details 调整 LED 亮度后，设置指定 LED 的颜色
 * @return 无
 */
void rgb_led_set_color(uint8_t index, rgb_color_t color)
{
    // 检查索引是否有效
    if (index >= RGB_LED_COUNT) return;

    // 1. 调整亮度，范围 0-255，0 表示关闭，255 表示最大亮度
    uint8_t r = (color.r * g_brightness) / 255;
    uint8_t g = (color.g * g_brightness) / 255;
    uint8_t b = (color.b * g_brightness) / 255;


    // 设置指定 LED 的颜色，根据颜色值为 WS2 WS2812 顺序
    esp_err_t ret = led_strip_set_pixel(g_led_strip, index, r, g, b);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置颜色失败: %d", ret);
        return;
    }
    // 刷新显示，将设置的颜色应用到 LED 条上
    ret = led_strip_refresh(g_led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "刷新显示失败: %d", ret);
        return;
    }


}

/**
 * @brief 设置全部灯珠颜色
 * @details 调整 LED 亮度后，设置所有 LED 的颜色
 * @return 无
 */
void rgb_led_set_all(rgb_color_t color)
{
    for (int i = 0; i < RGB_LED_COUNT; i++) {
        rgb_led_set_color(i, color);
    }
}

/**
 * @brief 关闭所有 LED
 * @details 将所有 LED 设置为黑色（关闭）
 * @return 无
 */
void rgb_led_clear(void)
{
    esp_err_t ret = led_strip_clear(g_led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "关闭 LED 失败: %d", ret);
    }
}