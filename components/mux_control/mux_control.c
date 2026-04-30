#include "mux_control.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define TAG "MUX_CONTROL"

// 当前各MUX选中的通道
static mux_channel_t current_channels[MUX_TYPE_COUNT] = {MUX_CHANNEL_0, MUX_CHANNEL_0};

// ==================== 内部：设置MUX选择引脚电平 ====================
static void set_mux_pins(gpio_num_t s1_pin, gpio_num_t s0_pin, mux_channel_t channel)
{
    gpio_set_level(s0_pin, channel & 0x01);       // S0 = bit0
    gpio_set_level(s1_pin, (channel >> 1) & 0x01); // S1 = bit1
}

// ==================== 初始化 ====================
esp_err_t mux_control_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MUX_RS485_S1_PIN) |
                        (1ULL << MUX_RS485_S0_PIN) |
                        (1ULL << MUX_RS232_S1_PIN) |
                        (1ULL << MUX_RS232_S0_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 默认选中通道0
    mux_select_channel(MUX_TYPE_RS485, MUX_CHANNEL_0);
    mux_select_channel(MUX_TYPE_RS232, MUX_CHANNEL_0);

    ESP_LOGI(TAG, "多路复用器初始化完成");
    ESP_LOGI(TAG, "  RS485 MUX(B): S1=GPIO%d, S0=GPIO%d", MUX_RS485_S1_PIN, MUX_RS485_S0_PIN);
    ESP_LOGI(TAG, "  RS232 MUX(A): S1=GPIO%d, S0=GPIO%d", MUX_RS232_S1_PIN, MUX_RS232_S0_PIN);

    return ESP_OK;
}

// ==================== 选择通道 ====================
esp_err_t mux_select_channel(mux_type_t mux_type, mux_channel_t channel)
{
    if (mux_type >= MUX_TYPE_COUNT) {
        ESP_LOGE(TAG, "无效的MUX类型: %d", mux_type);
        return ESP_ERR_INVALID_ARG;
    }
    if (channel >= MUX_CHANNEL_COUNT) {
        ESP_LOGE(TAG, "无效的通道号: %d", channel);
        return ESP_ERR_INVALID_ARG;
    }

    gpio_num_t s1_pin, s0_pin;
    const char *type_name;

    if (mux_type == MUX_TYPE_RS485) {
        s1_pin = MUX_RS485_S1_PIN;
        s0_pin = MUX_RS485_S0_PIN;
        type_name = "RS485";
    } else {
        s1_pin = MUX_RS232_S1_PIN;
        s0_pin = MUX_RS232_S0_PIN;
        type_name = "RS232";
    }

    set_mux_pins(s1_pin, s0_pin, channel);
    current_channels[mux_type] = channel;

    ESP_LOGI(TAG, "%s MUX 切换到通道 %d (S1=%d, S0=%d)",
             type_name, channel,
             (channel >> 1) & 0x01, channel & 0x01);

    return ESP_OK;
}

// ==================== 获取当前通道 ====================
int mux_get_channel(mux_type_t mux_type)
{
    if (mux_type >= MUX_TYPE_COUNT) {
        return -1;
    }
    return current_channels[mux_type];
}

// ==================== 切换到下一个通道（轮询用） ====================
mux_channel_t mux_next_channel(mux_type_t mux_type)
{
    if (mux_type >= MUX_TYPE_COUNT) {
        return MUX_CHANNEL_0;
    }

    mux_channel_t next = (current_channels[mux_type] + 1) % MUX_CHANNEL_COUNT;
    mux_select_channel(mux_type, next);
    return next;
}
