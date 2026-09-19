/*
 * FOC 的第一步：把三相 PWM 输出准备好。
 *
 * 这里做三件事：
 * 1. 把六个引脚交给芯片里的 PWM 定时器；
 * 2. 给 U、V、W 三个相位写入相同的 PWM 参数；
 * 3. 先把三个定时器关着。
 *
 * 函数执行完以后只完成准备工作，不会输出 PWM，也不会让电机转动。
 */

#include "foc_init.h"

#include "cy_device_headers.h"
#include "foc_hw_cyt2bl3.h"
#include "sysclk/cy_sysclk.h"
#include "system_cyt2bl.h"
#include "tcpwm/cy_tcpwm_pwm.h"
#include "zf_driver_gpio.h"

#include <string.h>

/*
 * 下面这些数值只是现在用来学习和检查波形的临时数值。
 *
 * 系统给 PWM 定时器的原始时钟是 80 MHz。先除以 10，得到 8 MHz，
 * 再让定时器从 0 数到 199、再从 199 数回 0，得到大约 20 kHz 的 PWM。
 * 这种“上数再下数”的方式叫中心对齐，波形两边比较对称。
 *
 * 20 kHz 和 0 个时钟周期的死区时间都不能直接当作最终功率级参数。
 * 接驱动器和 MOSFET 以前，必须根据实测关断时间重新设置死区。
 */
#define FOC_TCPWM_DIVIDER_INDEX       (0u)
#define FOC_TCPWM_DIVIDER_VALUE       (9u)  /* 芯片用 9 表示“实际除以 10”。 */
#define FOC_TCPWM_CLOCK_HZ            (CY_INITIAL_TARGET_PERI_FREQ / 10UL)
#define FOC_PWM_FREQUENCY_HZ          (20000UL)
#define FOC_PWM_PERIOD_TICKS          ((FOC_TCPWM_CLOCK_HZ / (FOC_PWM_FREQUENCY_HZ * 2UL)) - 1UL)
#define FOC_PWM_COMPARE_TICKS         ((FOC_PWM_PERIOD_TICKS + 1UL) / 2UL)
#define FOC_PWM_DEADTIME_TICKS        (0u)

/* 把一个物理引脚交给 PWM 定时器，之后由硬件自动输出 PWM。 */
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

/*
 * 让 U、V、W 三个 PWM 定时器共用同一个 8 MHz 时钟。
 *
 * CNT2、CNT3、CNT6 是芯片里对应 V、W、U 三相的三个小定时器。
 * 这里把它们都接到同一个分频器，只检查这个分频器已经由逐飞库打开，
 * 不重新改它的数值，因为系统延时也可能正在使用它。
 * 共用时钟只能保证三相走得一样快，还不能保证三相同一刻起跑；
 * 同步起跑要在后面再加一根公共触发线。
 */
static foc_init_status_t foc_init_tcpwm_clock(void)
{
    cy_en_sysclk_status_t status;

    status = Cy_SysClk_PeriphAssignDivider(
        PCLK_TCPWM0_CLOCKS514,
        CY_SYSCLK_DIV_16_BIT,
        FOC_TCPWM_DIVIDER_INDEX);
    if (status != CY_SYSCLK_SUCCESS)
    {
        return FOC_INIT_CLOCK_ERROR;
    }

    status = Cy_SysClk_PeriphAssignDivider(
        PCLK_TCPWM0_CLOCKS515,
        CY_SYSCLK_DIV_16_BIT,
        FOC_TCPWM_DIVIDER_INDEX);
    if (status != CY_SYSCLK_SUCCESS)
    {
        return FOC_INIT_CLOCK_ERROR;
    }

    status = Cy_SysClk_PeriphAssignDivider(
        PCLK_TCPWM0_CLOCKS518,
        CY_SYSCLK_DIV_16_BIT,
        FOC_TCPWM_DIVIDER_INDEX);
    if (status != CY_SYSCLK_SUCCESS)
    {
        return FOC_INIT_CLOCK_ERROR;
    }

    /* 芯片寄存器把“除以 10”保存成 19，这只是硬件的编码方式。 */
    if (!Cy_SysClk_PeriphGetDividerEnabled(
            CY_SYSCLK_DIV_16_BIT,
            FOC_TCPWM_DIVIDER_INDEX) ||
        (Cy_SysClk_PeriphGetDivider(
            CY_SYSCLK_DIV_16_BIT,
            FOC_TCPWM_DIVIDER_INDEX) !=
         ((FOC_TCPWM_DIVIDER_VALUE + 1u) * 2u - 1u)))
    {
        return FOC_INIT_CLOCK_ERROR;
    }

    return FOC_INIT_SUCCESS;
}

/* 给一个相位的 PWM 定时器写入三相共用的基础参数。 */
static foc_init_status_t foc_init_pwm_counter(
    volatile stc_TCPWM_GRP_CNT_t *counter)
{
    cy_stc_tcpwm_pwm_config_t config;
    uint32_t status;

    /* 先清零，避免没有填写的设置带着随机值。 */
    (void)memset(&config, 0, sizeof(config));

    /* 使用互补输出模式，让同一个定时器同时控制上、下桥臂。 */
    config.pwmMode            = CY_TCPWM_PWM_MODE_DEADTIME;
    config.clockPrescaler     = CY_TCPWM_PRESCALER_DIVBY_1;
    config.debug_pause        = true;
    config.deadTime           = FOC_PWM_DEADTIME_TICKS;
    config.deadTimeComp       = 0u;
    config.runMode            = CY_TCPWM_PWM_CONTINUOUS;
    /* 定时器先从 0 往上数，再往下数，PWM 波形因此中心对齐。 */
    config.countDirection     = CY_TCPWM_COUNTER_COUNT_UP_DOWN1;

    /* 到比较值时改变输出，到周期边界时恢复输出，形成 PWM 波形。 */
    config.cc0MatchMode       = CY_TCPWM_PWM_TR_CTRL2_CLEAR;
    config.overflowMode       = CY_TCPWM_PWM_TR_CTRL2_SET;
    config.underflowMode      = CY_TCPWM_PWM_TR_CTRL2_NO_CHANGE;
    config.cc1MatchMode       = CY_TCPWM_PWM_TR_CTRL2_NO_CHANGE;

    config.period             = FOC_PWM_PERIOD_TICKS;
    config.period_buff        = FOC_PWM_PERIOD_TICKS;
    config.enablePeriodSwap   = false;
    config.enableLineSelSwap  = false;
    config.compare0           = FOC_PWM_COMPARE_TICKS;
    config.compare0_buff      = FOC_PWM_COMPARE_TICKS;
    config.enableCompare0Swap = true;
    config.compare1           = 0u;
    config.compare1_buff      = 0u;
    config.enableCompare1Swap = false;
    config.interruptSources   = CY_TCPWM_INT_NONE;

    config.invertPWMOut       = CY_TCPWM_PWM_INVERT_DISABLE;
    config.invertPWMOutN      = CY_TCPWM_PWM_INVERT_DISABLE;
    config.killMode           = CY_TCPWM_PWM_STOP_ON_KILL;
    config.immediateKill      = true;
    config.pwmOnDisable       = CY_TCPWM_PWM_OUT_MODE_LOW;

    /* 现在先不用公共同步信号；三个定时器仍然保持关闭。 */
    config.switchInputMode    = CY_TCPWM_INPUT_RISING_EDGE;
    config.switchInput        = CY_TCPWM_INPUT0;
    config.reloadInputMode    = CY_TCPWM_INPUT_RISING_EDGE;
    config.reloadInput        = CY_TCPWM_INPUT0;
    config.startInputMode     = CY_TCPWM_INPUT_RISING_EDGE;
    config.startInput         = CY_TCPWM_INPUT0;
    config.kill0InputMode     = CY_TCPWM_INPUT_RISING_EDGE;
    config.kill0Input         = CY_TCPWM_INPUT0;
    config.kill1InputMode     = CY_TCPWM_INPUT_RISING_EDGE;
    config.kill1Input         = CY_TCPWM_INPUT0;
    config.countInputMode     = CY_TCPWM_INPUT_LEVEL;
    config.countInput         = CY_TCPWM_INPUT1;

    /* 现在还不让 PWM 自动启动 ADC，电流采样触发留到下一步。 */
    config.trigger0EventCfg   = CY_TCPWM_COUNTER_DISABLED;
    config.trigger1EventCfg   = CY_TCPWM_COUNTER_DISABLED;

    status = Cy_Tcpwm_Pwm_Init(counter, &config);
    if (status != CY_RET_SUCCESS)
    {
        return FOC_INIT_TCPWM_ERROR;
    }

    /* 明确关掉这个定时器，防止初始化阶段意外出现 PWM。 */
    Cy_Tcpwm_Pwm_Disable(counter);

    return FOC_INIT_SUCCESS;
}

foc_init_status_t foc_init(void)
{
    cy_en_gpio_status_t status;

    /* 先准备三相共用的时钟；这里仍然不启动任何 PWM。 */
    if (foc_init_tcpwm_clock() != FOC_INIT_SUCCESS)
    {
        return FOC_INIT_CLOCK_ERROR;
    }

    /* U 相：准备上桥臂和下桥臂两个输出脚。 */
    status = foc_init_route_pin(
        foc_motor1_hw.u_phase.line_pin,
        foc_motor1_hw.u_phase.line_hsiom);
    if (status != CY_GPIO_SUCCESS)
    {
        return FOC_INIT_GPIO_ERROR;
    }

    status = foc_init_route_pin(
        foc_motor1_hw.u_phase.line_compl_pin,
        foc_motor1_hw.u_phase.line_compl_hsiom);
    if (status != CY_GPIO_SUCCESS)
    {
        return FOC_INIT_GPIO_ERROR;
    }

    /* V 相：准备上桥臂和下桥臂两个输出脚。 */
    status = foc_init_route_pin(
        foc_motor1_hw.v_phase.line_pin,
        foc_motor1_hw.v_phase.line_hsiom);
    if (status != CY_GPIO_SUCCESS)
    {
        return FOC_INIT_GPIO_ERROR;
    }

    status = foc_init_route_pin(
        foc_motor1_hw.v_phase.line_compl_pin,
        foc_motor1_hw.v_phase.line_compl_hsiom);
    if (status != CY_GPIO_SUCCESS)
    {
        return FOC_INIT_GPIO_ERROR;
    }

    /* W 相：准备上桥臂和下桥臂两个输出脚。 */
    status = foc_init_route_pin(
        foc_motor1_hw.w_phase.line_pin,
        foc_motor1_hw.w_phase.line_hsiom);
    if (status != CY_GPIO_SUCCESS)
    {
        return FOC_INIT_GPIO_ERROR;
    }

    status = foc_init_route_pin(
        foc_motor1_hw.w_phase.line_compl_pin,
        foc_motor1_hw.w_phase.line_compl_hsiom);
    if (status != CY_GPIO_SUCCESS)
    {
        return FOC_INIT_GPIO_ERROR;
    }

    /* 三相使用完全相同的计数规则，但现在都保持关闭。 */
    if (foc_init_pwm_counter(TCPWM0_GRP2_CNT2) != FOC_INIT_SUCCESS)
    {
        return FOC_INIT_TCPWM_ERROR;
    }

    if (foc_init_pwm_counter(TCPWM0_GRP2_CNT3) != FOC_INIT_SUCCESS)
    {
        return FOC_INIT_TCPWM_ERROR;
    }

    if (foc_init_pwm_counter(TCPWM0_GRP2_CNT6) != FOC_INIT_SUCCESS)
    {
        return FOC_INIT_TCPWM_ERROR;
    }

    return FOC_INIT_SUCCESS;
}
