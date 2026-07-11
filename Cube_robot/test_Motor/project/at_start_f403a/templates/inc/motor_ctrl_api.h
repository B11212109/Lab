#ifndef MOTOR_CTRL_API_H
#define MOTOR_CTRL_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MOTOR_RET_OK = 0,
    MOTOR_RET_INVALID_ARG = -1,
    MOTOR_RET_BUSY = -2,
    MOTOR_RET_NOT_READY = -3
} motor_ret_t;

typedef struct {
    float v_start_pps;
    float v_max_pps;
    float accel_pps2;
} motor_profile_t;

motor_ret_t motor_init(void);
motor_ret_t motor_set_profile(motor_profile_t profile);
motor_ret_t motor_turn(int32_t degree);
uint32_t motor_degree_to_steps(int32_t degree);

#ifdef __cplusplus
}
#endif

#endif
