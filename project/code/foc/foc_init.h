/*
 * CYT2BL3 FOC 底层初始化
 *
 * 当前版本只初始化电机 1 的 GPIO 复用。
 * 这里还没有配置 TCPWM、ADC，也不会主动输出 PWM。
 */

#ifndef FOC_INIT_H
#define FOC_INIT_H

#include "cy_gpio.h"

/* 返回 GPIO 初始化结果；调用者必须检查返回值。 */
cy_en_gpio_status_t foc_init(void);

#endif /* FOC_INIT_H */
