#include "motor_ctrl_api.h"

int motor_unit_test_run(void)
{
    motor_profile_t p = {500.0f, 5000.0f, 500000.0f};

    if (motor_init() != MOTOR_RET_OK) {
        return -1;
    }

    if (motor_set_profile(p) != MOTOR_RET_OK) {
        return -2;
    }

    if (motor_turn(90) != MOTOR_RET_OK) {
        return -3;
    }

    if (motor_turn(180) != MOTOR_RET_OK) {
        return -4;
    }

    return 0;
}
