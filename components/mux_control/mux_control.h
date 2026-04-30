#ifndef MUX_CONTROL_H
#define MUX_CONTROL_H

#include "esp_err.h"
#include "driver/gpio.h"

// ==================== 引脚配置 ====================
// 74HC4052PW118 B (RS485) 控制引脚
#define MUX_RS485_S1_PIN    GPIO_NUM_14   // 74HC4052 B 的 S1
#define MUX_RS485_S0_PIN    GPIO_NUM_15   // 74HC4052 B 的 S0

// 74HC4052PW118 A (RS232) 控制引脚
#define MUX_RS232_S1_PIN    GPIO_NUM_12   // 74HC4052 A 的 S1
#define MUX_RS232_S0_PIN    GPIO_NUM_13   // 74HC4052 A 的 S0

// 74HC4052 通道选择真值表:
//   S1  S0  →  通道
//    0   0  →  CH0 (Y0)
//    0   1  →  CH1 (Y1)
//    1   0  →  CH2 (Y2)
//    1   1  →  CH3 (Y3)

// ==================== MUX 类型 ====================
typedef enum {
    MUX_TYPE_RS485 = 0,   // 74HC4052PW118 B
    MUX_TYPE_RS232 = 1,   // 74HC4052PW118 A
    MUX_TYPE_COUNT = 2
} mux_type_t;

// ==================== 通道号 ====================
typedef enum {
    MUX_CHANNEL_0 = 0,
    MUX_CHANNEL_1 = 1,
    MUX_CHANNEL_2 = 2,
    MUX_CHANNEL_3 = 3,
    MUX_CHANNEL_COUNT = 4
} mux_channel_t;

// ==================== 对外接口 ====================

/**
 * @brief  初始化多路复用器控制（配置GPIO，默认选中通道0）
 */
esp_err_t mux_control_init(void);

/**
 * @brief  选择指定MUX的通道
 * @param  mux_type  MUX类型: MUX_TYPE_RS485 或 MUX_TYPE_RS232
 * @param  channel   通道号: 0~3
 */
esp_err_t mux_select_channel(mux_type_t mux_type, mux_channel_t channel);

/**
 * @brief  获取指定MUX当前通道号
 * @return 通道号 0~3，错误返回 -1
 */
int mux_get_channel(mux_type_t mux_type);

/**
 * @brief  切换到下一个通道（用于轮询）
 * @param  mux_type  MUX类型
 * @return 切换后的通道号
 */
mux_channel_t mux_next_channel(mux_type_t mux_type);

#endif // MUX_CONTROL_H
