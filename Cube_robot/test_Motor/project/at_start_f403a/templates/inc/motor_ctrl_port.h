#ifndef MOTOR_CTRL_PORT_H
#define MOTOR_CTRL_PORT_H

#include <stdint.h>
#include "motor_ctrl_api.h"

#ifdef __cplusplus
extern "C" {
#endif

motor_ret_t motor_port_hw_init(void);
void motor_port_set_enable(uint8_t en);
void motor_port_set_direction(uint8_t cw);
motor_ret_t motor_port_emit_steps(uint32_t steps, motor_profile_t profile);

#ifdef __cplusplus
}
#endif

#endif
