/*
 * CYT2BL3 FOC 底层初始化
 *
 * 这一步只做“引脚复用”：
 *   P14.0/P14.1 -> TCPWM0_LINE518 / LINE_COMPL518
 *   P18.4/P18.5 -> TCPWM0_LINE514 / LINE_COMPL514
 *   P18.6/P18.7 -> TCPWM0_LINE515 / LINE_COMPL515
 *
 * 引脚复用只是把引脚接到 TCPWM 外设信号线上。
 * TCPWM counter 还没有配置或启动，因此这里不会产生 PWM 波形。
 */

#include "foc_init.h"

#include "cy_device_headers.h"
#include "foc_hw_cyt2bl3.h"
#include "zf_driver_gpio.h"

/*
 * 给一个引脚选择 TCPWM 外设功能。
 *
 * 这个函数直接调用官方 PDL 的 Cy_GPIO_Pin_Init，方便逐项学习它最终
 * 修改的 GPIO/HSIOM 寄存器。它不是新的硬件抽象层，只是初始化过程中的
 * 一个局部重复代码整理。
 */
static cy_en_gpio_status_t foc_init_route_pin(
    gpio_pin_enum pin,
    en_hsiom_sel_t hsiom)
{
    cy_stc_gpio_pin_config_t pin_config = {0};

    pin_config.driveMode = CY_GPIO_DM_STRONG;
    pin_config.hsiom = hsiom;
    pin_config.slewRate = CY_GPIO_SLEW_FAST;
    pin_config.driveSel = CY_GPIO_DRIVE_FULL;

    return Cy_GPIO_Pin_Init(
        get_port(pin),
        (uint32_t)(pin % 8u),
        &pin_config);
}

cy_en_gpio_status_t foc_init(void)
{
    cy_en_gpio_status_t status;

    /* 先配置 U 相半桥的上、下两个输入。 */
    status = foc_init_route_pin(
        foc_motor1_hw.u_phase.line_pin,
        foc_motor1_hw.u_phase.line_hsiom);
    if (status != CY_GPIO_SUCCESS)
    {
        return status;
    }

    status = foc_init_route_pin(
        foc_motor1_hw.u_phase.line_compl_pin,
        foc_motor1_hw.u_phase.line_compl_hsiom);
    if (status != CY_GPIO_SUCCESS)
    {
        return status;
    }

    /* 再配置 V 相半桥的上、下两个输入。 */
    status = foc_init_route_pin(
        foc_motor1_hw.v_phase.line_pin,
        foc_motor1_hw.v_phase.line_hsiom);
    if (status != CY_GPIO_SUCCESS)
    {
        return status;
    }

    status = foc_init_route_pin(
        foc_motor1_hw.v_phase.line_compl_pin,
        foc_motor1_hw.v_phase.line_compl_hsiom);
    if (status != CY_GPIO_SUCCESS)
    {
        return status;
    }

    /* 最后配置 W 相半桥的上、下两个输入。 */
    status = foc_init_route_pin(
        foc_motor1_hw.w_phase.line_pin,
        foc_motor1_hw.w_phase.line_hsiom);
    if (status != CY_GPIO_SUCCESS)
    {
        return status;
    }

    return foc_init_route_pin(
        foc_motor1_hw.w_phase.line_compl_pin,
        foc_motor1_hw.w_phase.line_compl_hsiom);
}
