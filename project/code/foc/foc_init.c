#include "foc_init.h"

#include "cy_device_headers.h"
#include "foc_hw_cyt2bl3.h"
#include "sysclk/cy_sysclk.h"
#include "system_cyt2bl.h"
#include "tcpwm/cy_tcpwm.h"
#include "tcpwm/cy_tcpwm_pwm.h"
#include "trigmux/cy_trigmux.h"
#include "zf_driver_gpio.h"

#include <string.h>

//-------------------------------------------------------------------------------------------------------------------
// 宏定义简介     当前 PWM 测试参数
// 备注信息     80 MHz 外设时钟先除以 10，得到 8 MHz
//              定时器从 0 数到 199，再从 199 数回 0，得到约 20 kHz 中心对齐 PWM
//              比较值取一半，占空比约 50%
//              死区 8 个时钟周期 = 1.0 us，只是看波起步值，以后要按 MOSFET 实测关断时间重设
//-------------------------------------------------------------------------------------------------------------------
#define FOC_TCPWM_DIVIDER_INDEX       (0u)
#define FOC_TCPWM_DIVIDER_VALUE       (9u)                                      // 芯片用 9 表示实际除以 10
#define FOC_TCPWM_CLOCK_HZ            (CY_INITIAL_TARGET_PERI_FREQ / 10UL)
#define FOC_PWM_FREQUENCY_HZ          (20000UL)
#define FOC_PWM_PERIOD_TICKS          ((FOC_TCPWM_CLOCK_HZ / (FOC_PWM_FREQUENCY_HZ * 2UL)) - 1UL)
#define FOC_PWM_COMPARE_TICKS         ((FOC_PWM_PERIOD_TICKS + 1UL) / 2UL)
#define FOC_PWM_DEADTIME_TICKS        (8u)

static foc_init_status_t foc_init_pwm_counter(volatile stc_TCPWM_GRP_CNT_t *counter);

//-------------------------------------------------------------------------------------------------------------------
// 函数简介     把一个引脚交给 PWM 定时器
// 参数说明     pin             物理引脚
// 参数说明     hsiom           这个脚接到 LINE 还是 LINE_COMPL
// 返回参数     cy_en_gpio_status_t     引脚是否配置成功
// 备注信息     配好以后由硬件自动输出 PWM，不再当普通 GPIO 翻转
//-------------------------------------------------------------------------------------------------------------------
static cy_en_gpio_status_t foc_init_route_pin(gpio_pin_enum pin, en_hsiom_sel_t hsiom)
{
    cy_stc_gpio_pin_config_t pin_config = {0};

    pin_config.driveMode = CY_GPIO_DM_STRONG;
    pin_config.hsiom = hsiom;
    pin_config.slewRate = CY_GPIO_SLEW_FAST;
    pin_config.driveSel = CY_GPIO_DRIVE_FULL;

    return Cy_GPIO_Pin_Init(get_port(pin), (uint32_t)(pin % 8u), &pin_config);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数简介     给一个半桥接上 8 MHz PWM 时钟
// 参数说明     phase           这一相的硬件表
// 返回参数     foc_init_status_t
// 备注信息     只接到逐飞库已经打开的分频器，不改分频值，因为系统延时也可能在用它
//-------------------------------------------------------------------------------------------------------------------
static foc_init_status_t foc_assign_pwm_clock(const foc_half_bridge_hw_t *phase)
{
    if (Cy_SysClk_PeriphAssignDivider(
            phase->pwm_clock,
            CY_SYSCLK_DIV_16_BIT,
            FOC_TCPWM_DIVIDER_INDEX) != CY_SYSCLK_SUCCESS)
    {
        return FOC_INIT_CLOCK_ERROR;
    }

    return FOC_INIT_SUCCESS;
}

//-------------------------------------------------------------------------------------------------------------------
// 函数简介     准备一个半桥的上、下桥引脚
// 参数说明     phase           这一相的硬件表
// 返回参数     foc_init_status_t
//-------------------------------------------------------------------------------------------------------------------
static foc_init_status_t foc_init_phase_pins(const foc_half_bridge_hw_t *phase)
{
    if (foc_init_route_pin(phase->line_pin, phase->line_hsiom) != CY_GPIO_SUCCESS)
    {
        return FOC_INIT_GPIO_ERROR;
    }

    if (foc_init_route_pin(phase->line_compl_pin, phase->line_compl_hsiom) != CY_GPIO_SUCCESS)
    {
        return FOC_INIT_GPIO_ERROR;
    }

    return FOC_INIT_SUCCESS;
}

//-------------------------------------------------------------------------------------------------------------------
// 函数简介     检查 PWM 共用分频器是不是 8 MHz
// 参数说明     void
// 返回参数     foc_init_status_t
// 备注信息     芯片寄存器把“除以 10”保存成 19，这只是硬件编码方式
//-------------------------------------------------------------------------------------------------------------------
static foc_init_status_t foc_check_pwm_divider(void)
{
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

//-------------------------------------------------------------------------------------------------------------------
// 函数简介     准备一台电机的时钟、引脚和三个 PWM 定时器
// 参数说明     motor           电机 1 或电机 2 的硬件表
// 返回参数     foc_init_status_t
// 备注信息     定时器配完后保持关闭，这里还不会出波形
//-------------------------------------------------------------------------------------------------------------------
static foc_init_status_t foc_init_motor(const foc_motor_hw_t *motor)
{
    if (foc_assign_pwm_clock(&motor->u_phase) != FOC_INIT_SUCCESS)
    {
        return FOC_INIT_CLOCK_ERROR;
    }

    if (foc_assign_pwm_clock(&motor->v_phase) != FOC_INIT_SUCCESS)
    {
        return FOC_INIT_CLOCK_ERROR;
    }

    if (foc_assign_pwm_clock(&motor->w_phase) != FOC_INIT_SUCCESS)
    {
        return FOC_INIT_CLOCK_ERROR;
    }

    if (foc_init_phase_pins(&motor->u_phase) != FOC_INIT_SUCCESS)
    {
        return FOC_INIT_GPIO_ERROR;
    }

    if (foc_init_phase_pins(&motor->v_phase) != FOC_INIT_SUCCESS)
    {
        return FOC_INIT_GPIO_ERROR;
    }

    if (foc_init_phase_pins(&motor->w_phase) != FOC_INIT_SUCCESS)
    {
        return FOC_INIT_GPIO_ERROR;
    }

    if (foc_init_pwm_counter(motor->u_phase.tcpwm) != FOC_INIT_SUCCESS)
    {
        return FOC_INIT_TCPWM_ERROR;
    }

    if (foc_init_pwm_counter(motor->v_phase.tcpwm) != FOC_INIT_SUCCESS)
    {
        return FOC_INIT_TCPWM_ERROR;
    }

    if (foc_init_pwm_counter(motor->w_phase.tcpwm) != FOC_INIT_SUCCESS)
    {
        return FOC_INIT_TCPWM_ERROR;
    }

    return FOC_INIT_SUCCESS;
}

//-------------------------------------------------------------------------------------------------------------------
// 函数简介     打开一台电机的三路 PWM，先不算齐
// 参数说明     motor           电机 1 或电机 2 的硬件表
// 返回参数     void
//-------------------------------------------------------------------------------------------------------------------
static void foc_pwm_enable_motor(const foc_motor_hw_t *motor)
{
    Cy_Tcpwm_Pwm_Enable(motor->u_phase.tcpwm);
    Cy_Tcpwm_Pwm_Enable(motor->v_phase.tcpwm);
    Cy_Tcpwm_Pwm_Enable(motor->w_phase.tcpwm);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数简介     每路自己 TriggerStart，作为同步失败时的退路
// 参数说明     motor           电机 1 或电机 2 的硬件表
// 返回参数     void
// 备注信息     和逐飞 pwm_init() 同一条路，能出波但不保证三相对齐
//-------------------------------------------------------------------------------------------------------------------
static void foc_pwm_start_motor(const foc_motor_hw_t *motor)
{
    Cy_Tcpwm_TriggerStart(motor->u_phase.tcpwm);
    Cy_Tcpwm_TriggerStart(motor->v_phase.tcpwm);
    Cy_Tcpwm_TriggerStart(motor->w_phase.tcpwm);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数简介     给一个 PWM 定时器写入三相共用的基础参数
// 参数说明     counter         这一相的 TCPWM 定时器
// 返回参数     foc_init_status_t
// 备注信息     互补输出 + 中心对齐 + 1 us 死区 + 固定约 50% 占空比
//              配完后立刻关掉，防止初始化阶段意外出波
//-------------------------------------------------------------------------------------------------------------------
static foc_init_status_t foc_init_pwm_counter(volatile stc_TCPWM_GRP_CNT_t *counter)
{
    cy_stc_tcpwm_pwm_config_t config;
    uint32_t status;

    (void)memset(&config, 0, sizeof(config));

    config.pwmMode            = CY_TCPWM_PWM_MODE_DEADTIME;                     // 一个定时器同时管上、下桥
    config.clockPrescaler     = CY_TCPWM_PRESCALER_DIVBY_1;
    config.debug_pause        = true;                                           // 调试停核时 PWM 冻结，看波不要停在断点上
    config.deadTime           = FOC_PWM_DEADTIME_TICKS;
    config.deadTimeComp       = 0u;
    config.runMode            = CY_TCPWM_PWM_CONTINUOUS;
    config.countDirection     = CY_TCPWM_COUNTER_COUNT_UP_DOWN1;                // 上数再下数，波形中心对齐

    config.cc0MatchMode       = CY_TCPWM_PWM_TR_CTRL2_CLEAR;                    // 到比较值时翻转输出
    config.overflowMode       = CY_TCPWM_PWM_TR_CTRL2_SET;                      // 到周期边界时恢复输出
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
    config.pwmOnDisable       = CY_TCPWM_PWM_OUT_MODE_LOW;                      // 关掉定时器时输出拉低

    config.switchInputMode    = CY_TCPWM_INPUT_RISING_EDGE;
    config.switchInput        = CY_TCPWM_INPUT0;
    config.reloadInputMode    = CY_TCPWM_INPUT_RISING_EDGE;
    config.reloadInput        = CY_TCPWM_INPUT_TRIG2;                           // 听公共触发线 tr_all_cnt_in[2]
    config.startInputMode     = CY_TCPWM_INPUT_RISING_EDGE;
    config.startInput         = CY_TCPWM_INPUT0;
    config.kill0InputMode     = CY_TCPWM_INPUT_RISING_EDGE;
    config.kill0Input         = CY_TCPWM_INPUT0;
    config.kill1InputMode     = CY_TCPWM_INPUT_RISING_EDGE;
    config.kill1Input         = CY_TCPWM_INPUT0;
    config.countInputMode     = CY_TCPWM_INPUT_LEVEL;
    config.countInput         = CY_TCPWM_INPUT1;

    config.trigger0EventCfg   = CY_TCPWM_COUNTER_DISABLED;                      // 电流采样触发留到下一步
    config.trigger1EventCfg   = CY_TCPWM_COUNTER_DISABLED;

    status = Cy_Tcpwm_Pwm_Init(counter, &config);
    if (status != CY_RET_SUCCESS)
    {
        return FOC_INIT_TCPWM_ERROR;
    }

    Cy_Tcpwm_Pwm_Disable(counter);
    return FOC_INIT_SUCCESS;
}

//-------------------------------------------------------------------------------------------------------------------
// 函数简介     启动两台电机的六路 PWM
// 参数说明     void
// 返回参数     void
// 使用示例     foc_pwm_start();
// 备注信息     先打开六路，再打同一根触发线同时 reload，让同电机三相对齐
//              公共触发失败时退回每路 TriggerStart，六路仍会出波
//-------------------------------------------------------------------------------------------------------------------
void foc_pwm_start(void)
{
    foc_pwm_enable_motor(&foc_motor1_hw);
    foc_pwm_enable_motor(&foc_motor2_hw);

    if (Cy_TrigMux_SwTrigger(
            TRIG_OUT_MUX_4_TCPWM_ALL_CNT_TR_IN2,
            TRIGGER_TYPE_EDGE,
            1ul) != CY_TRIGMUX_SUCCESS)
    {
        foc_pwm_start_motor(&foc_motor1_hw);
        foc_pwm_start_motor(&foc_motor2_hw);
    }
}

//-------------------------------------------------------------------------------------------------------------------
// 函数简介     准备两台电机的三相 PWM
// 参数说明     void
// 返回参数     foc_init_status_t       准备结果，失败时不要再启动 PWM
// 使用示例     if (foc_init() != FOC_INIT_SUCCESS) { for(;;){} }
// 备注信息     先检查 8 MHz 分频器，再按硬件表准备电机 1、电机 2
//              返回成功时定时器仍然关闭，不会主动输出 PWM
//-------------------------------------------------------------------------------------------------------------------
foc_init_status_t foc_init(void)
{
    foc_init_status_t status;

    if (foc_check_pwm_divider() != FOC_INIT_SUCCESS)
    {
        return FOC_INIT_CLOCK_ERROR;
    }

    status = foc_init_motor(&foc_motor1_hw);
    if (status != FOC_INIT_SUCCESS)
    {
        return status;
    }

    status = foc_init_motor(&foc_motor2_hw);
    if (status != FOC_INIT_SUCCESS)
    {
        return status;
    }

    return FOC_INIT_SUCCESS;
}
