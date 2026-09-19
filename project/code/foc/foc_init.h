/*
 * FOC 第一步：准备电机 1 的六个 PWM 引脚和三个 PWM 定时器。
 * 函数返回时定时器仍然关闭，所以不会主动输出 PWM。
 */

#ifndef FOC_INIT_H
#define FOC_INIT_H

#include "gpio/cy_gpio.h"

/* 初始化失败时，用这个结果区分是引脚、时钟还是 PWM 定时器配置出错。 */
typedef enum
{
    FOC_INIT_SUCCESS     = 0u, /* 全部准备成功。 */
    FOC_INIT_GPIO_ERROR  = 1u, /* 某个引脚准备失败。 */
    FOC_INIT_CLOCK_ERROR = 2u, /* PWM 时钟准备失败。 */
    FOC_INIT_TCPWM_ERROR = 3u, /* PWM 定时器配置失败。 */
} foc_init_status_t;

/* 返回准备结果；主函数必须检查，失败时不能继续运行。 */
foc_init_status_t foc_init(void);

#endif /* FOC_INIT_H */
