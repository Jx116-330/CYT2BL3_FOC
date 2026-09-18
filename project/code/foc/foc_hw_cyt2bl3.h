/*
 * CYT2BL3 FOC 硬件资源表
 *
 * FOC 的第一步刻意只建立数据表。
 * 它记录原理图和 MCU 外设资源之间的对应关系，
 * 方便后续寄存器代码对照这张稳定的小表进行检查。
 * 引入这个文件不会使能外设，也不会驱动任何引脚。
 */

#ifndef FOC_HW_CYT2BL3_H
#define FOC_HW_CYT2BL3_H

#include <stdint.h>

#include "cy_device_headers.h"
#include "zf_driver_adc.h"
#include "zf_driver_gpio.h"

/* 一个半桥使用一个 TCPWM counter，以及它对应的 LINE/LINE_COMPL 输出对。 */
typedef struct
{
    uint8_t         tcpwm_group;          /* TCPWM0 的 group 编号，例如 0、1、2。 */
    uint8_t         tcpwm_counter;        /* 该 group 内部的 counter 编号。 */
    uint16_t        tcpwm_line;           /* 逻辑 LINE 编号，例如 518。 */
    uint16_t        tcpwm_line_compl;     /* 逻辑互补 LINE 编号。 */
    gpio_pin_enum   line_pin;             /* 外部连接到上桥驱动输入的物理引脚。 */
    gpio_pin_enum   line_compl_pin;       /* 外部连接到下桥驱动输入的物理引脚。 */
    en_hsiom_sel_t  line_hsiom;           /* 上桥引脚使用的 HSIOM 选择值。 */
    en_hsiom_sel_t  line_compl_hsiom;     /* 下桥引脚使用的 HSIOM 选择值。 */
} foc_half_bridge_hw_t;

/*
 * 板上实际测量 U 相和 W 相电流。
 * ADC 通路验证完成后，软件再使用 I_v = -(I_u + I_w) 重构 V 相电流。
 */
typedef struct
{
    foc_half_bridge_hw_t u_phase;
    foc_half_bridge_hw_t v_phase;
    foc_half_bridge_hw_t w_phase;
    adc_channel_enum     current_u_adc;
    adc_channel_enum     current_w_adc;
    uint8_t              current_sar;     /* 1 表示 SAR1，2 表示 SAR2。 */
} foc_motor_hw_t;

/*
 * 电机 1，来自原理图的连接关系：
 *
 *   U: P14.0/P14.1 -> FD6288T HIN1/LIN1
 *   V: P18.4/P18.5 -> FD6288T HIN2/LIN2
 *   W: P18.6/P18.7 -> FD6288T HIN3/LIN3
 *
 * 这里使用扩展 HSIOM 选择值 16 是有意的。
 * 它们把同一个 TCPWM counter 的 LINE 和 LINE_COMPL 引到两个引脚，
 * 这是实现互补 PWM 和硬件 dead-time 的必要条件。
 */
static const foc_motor_hw_t foc_motor1_hw =
{
    .u_phase =
    {
        .tcpwm_group       = 2u,
        .tcpwm_counter     = 6u,
        .tcpwm_line        = 518u,
        .tcpwm_line_compl  = 518u,
        .line_pin          = P14_0,
        .line_compl_pin    = P14_1,
        .line_hsiom        = P14_0_TCPWM0_LINE518,
        .line_compl_hsiom  = P14_1_TCPWM0_LINE_COMPL518,
    },
    .v_phase =
    {
        .tcpwm_group       = 2u,
        .tcpwm_counter     = 2u,
        .tcpwm_line        = 514u,
        .tcpwm_line_compl  = 514u,
        .line_pin          = P18_4,
        .line_compl_pin    = P18_5,
        .line_hsiom        = P18_4_TCPWM0_LINE514,
        .line_compl_hsiom  = P18_5_TCPWM0_LINE_COMPL514,
    },
    .w_phase =
    {
        .tcpwm_group       = 2u,
        .tcpwm_counter     = 3u,
        .tcpwm_line        = 515u,
        .tcpwm_line_compl  = 515u,
        .line_pin          = P18_6,
        .line_compl_pin    = P18_7,
        .line_hsiom        = P18_6_TCPWM0_LINE515,
        .line_compl_hsiom  = P18_7_TCPWM0_LINE_COMPL515,
    },
    .current_u_adc = ADC2_CH01_P18_1,
    .current_w_adc = ADC2_CH00_P18_0,
    .current_sar   = 2u,
};

/*
 * 电机 2 先记录在这里，但要等电机 1 的 PWM/ADC 通路验证完成后再初始化。
 * 两个电机都集中记录在这里，可以避免后续驱动代码重复填写引脚分配。
 */
static const foc_motor_hw_t foc_motor2_hw =
{
    .u_phase =
    {
        .tcpwm_group       = 0u,
        .tcpwm_counter     = 14u,
        .tcpwm_line        = 14u,
        .tcpwm_line_compl  = 14u,
        .line_pin          = P00_2,
        .line_compl_pin    = P00_3,
        .line_hsiom        = P0_2_TCPWM0_LINE14,
        .line_compl_hsiom  = P0_3_TCPWM0_LINE_COMPL14,
    },
    .v_phase =
    {
        .tcpwm_group       = 0u,
        .tcpwm_counter     = 7u,
        .tcpwm_line        = 7u,
        .tcpwm_line_compl  = 7u,
        .line_pin          = P02_0,
        .line_compl_pin    = P02_1,
        .line_hsiom        = P2_0_TCPWM0_LINE7,
        .line_compl_hsiom  = P2_1_TCPWM0_LINE_COMPL7,
    },
    .w_phase =
    {
        .tcpwm_group       = 0u,
        .tcpwm_counter     = 9u,
        .tcpwm_line        = 9u,
        .tcpwm_line_compl  = 9u,
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
