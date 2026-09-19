#ifndef FOC_HW_CYT2BL3_H
#define FOC_HW_CYT2BL3_H

#include <stdint.h>

#include "cy_device_headers.h"
#include "zf_driver_adc.h"
#include "zf_driver_gpio.h"

//-------------------------------------------------------------------------------------------------------------------
// 结构简介     一个半桥的硬件资源
// 参数说明     tcpwm               控制这一相的 PWM 定时器
// 参数说明     pwm_clock           这个定时器的外设时钟
// 参数说明     line_pin            上桥驱动输入脚，接到 FD6288T 的 HIN
// 参数说明     line_compl_pin      下桥驱动输入脚，接到 FD6288T 的 LIN
// 参数说明     line_hsiom          上桥脚接到 PWM 正输出 LINE
// 参数说明     line_compl_hsiom    下桥脚接到 PWM 互补输出 LINE_COMPL
// 备注信息     一个定时器同时出 LINE 和 LINE_COMPL，才能做硬件互补和死区
//-------------------------------------------------------------------------------------------------------------------
typedef struct
{
    volatile stc_TCPWM_GRP_CNT_t *tcpwm;
    en_clk_dst_t               pwm_clock;
    gpio_pin_enum              line_pin;
    gpio_pin_enum              line_compl_pin;
    en_hsiom_sel_t             line_hsiom;
    en_hsiom_sel_t             line_compl_hsiom;
} foc_half_bridge_hw_t;

//-------------------------------------------------------------------------------------------------------------------
// 结构简介     一台电机的硬件资源
// 参数说明     u_phase / v_phase / w_phase     三相半桥
// 参数说明     current_u_adc / current_w_adc   板上实测的 U、W 相电流通道
// 参数说明     current_sar                     1 = SAR1，2 = SAR2
// 备注信息     电流采样还没做。以后用 I_v = -(I_u + I_w) 软件补出 V 相
//-------------------------------------------------------------------------------------------------------------------
typedef struct
{
    foc_half_bridge_hw_t u_phase;
    foc_half_bridge_hw_t v_phase;
    foc_half_bridge_hw_t w_phase;
    adc_channel_enum     current_u_adc;
    adc_channel_enum     current_w_adc;
    uint8_t              current_sar;
} foc_motor_hw_t;

//-------------------------------------------------------------------------------------------------------------------
// 变量简介     电机 1 接线表
// 备注信息     U: P14.0/P14.1 -> HIN1/LIN1
//              V: P18.4/P18.5 -> HIN2/LIN2
//              W: P18.6/P18.7 -> HIN3/LIN3
//              改接线只改这张表，foc_init.c 按表准备 PWM
//-------------------------------------------------------------------------------------------------------------------
static const foc_motor_hw_t foc_motor1_hw =
{
    .u_phase =
    {
        .tcpwm             = TCPWM0_GRP2_CNT6,
        .pwm_clock         = PCLK_TCPWM0_CLOCKS518,
        .line_pin          = P14_0,
        .line_compl_pin    = P14_1,
        .line_hsiom        = P14_0_TCPWM0_LINE518,
        .line_compl_hsiom  = P14_1_TCPWM0_LINE_COMPL518,
    },
    .v_phase =
    {
        .tcpwm             = TCPWM0_GRP2_CNT2,
        .pwm_clock         = PCLK_TCPWM0_CLOCKS514,
        .line_pin          = P18_4,
        .line_compl_pin    = P18_5,
        .line_hsiom        = P18_4_TCPWM0_LINE514,
        .line_compl_hsiom  = P18_5_TCPWM0_LINE_COMPL514,
    },
    .w_phase =
    {
        .tcpwm             = TCPWM0_GRP2_CNT3,
        .pwm_clock         = PCLK_TCPWM0_CLOCKS515,
        .line_pin          = P18_6,
        .line_compl_pin    = P18_7,
        .line_hsiom        = P18_6_TCPWM0_LINE515,
        .line_compl_hsiom  = P18_7_TCPWM0_LINE_COMPL515,
    },
    .current_u_adc = ADC2_CH01_P18_1,
    .current_w_adc = ADC2_CH00_P18_0,
    .current_sar   = 2u,
};

//-------------------------------------------------------------------------------------------------------------------
// 变量简介     电机 2 接线表
// 备注信息     U: P00.2/P00.3 -> HIN1/LIN1
//              V: P02.0/P02.1 -> HIN2/LIN2
//              W: P05.0/P05.1 -> HIN3/LIN3
//-------------------------------------------------------------------------------------------------------------------
static const foc_motor_hw_t foc_motor2_hw =
{
    .u_phase =
    {
        .tcpwm             = TCPWM0_GRP0_CNT14,
        .pwm_clock         = PCLK_TCPWM0_CLOCKS14,
        .line_pin          = P00_2,
        .line_compl_pin    = P00_3,
        .line_hsiom        = P0_2_TCPWM0_LINE14,
        .line_compl_hsiom  = P0_3_TCPWM0_LINE_COMPL14,
    },
    .v_phase =
    {
        .tcpwm             = TCPWM0_GRP0_CNT7,
        .pwm_clock         = PCLK_TCPWM0_CLOCKS7,
        .line_pin          = P02_0,
        .line_compl_pin    = P02_1,
        .line_hsiom        = P2_0_TCPWM0_LINE7,
        .line_compl_hsiom  = P2_1_TCPWM0_LINE_COMPL7,
    },
    .w_phase =
    {
        .tcpwm             = TCPWM0_GRP0_CNT9,
        .pwm_clock         = PCLK_TCPWM0_CLOCKS9,
        .line_pin          = P05_0,
        .line_compl_pin    = P05_1,
        .line_hsiom        = P5_0_TCPWM0_LINE9,
        .line_compl_hsiom  = P5_1_TCPWM0_LINE_COMPL9,
    },
    .current_u_adc = ADC1_CH04_P12_0,
    .current_w_adc = ADC1_CH05_P12_1,
    .current_sar   = 1u,
};

#endif /* FOC_HW_CYT2BL3_H */
