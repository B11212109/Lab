#include "motor_ctrl_api.h"
#include "motor_ctrl_cfg.h"
#include "motor_ctrl_port.h"

#include <stdbool.h>

typedef struct {
    bool inited;
    motor_profile_t profile;
} motor_ctrl_ctx_t;

static motor_ctrl_ctx_t g_motor_ctx = {
    false,
    {MOTOR_V_START_PPS, MOTOR_V_MAX_PPS, MOTOR_ACCEL_PPS2}
};

static bool motor_profile_valid(motor_profile_t profile)
{
    if (profile.v_start_pps <= 0.0f || profile.v_max_pps <= 0.0f || profile.accel_pps2 <= 0.0f) {
        return false;
    }
    if (profile.v_max_pps < profile.v_start_pps) {
        return false;
    }
    return true;
}

motor_ret_t motor_init(void)
{
    motor_ret_t ret = motor_port_hw_init();
    if (ret != MOTOR_RET_OK) {
        return ret;
    }

    /* Keep motor disabled at idle to avoid unnecessary heating. */
    motor_port_set_enable(0U);
    g_motor_ctx.inited = true;
    return MOTOR_RET_OK;
}

motor_ret_t motor_set_profile(motor_profile_t profile)
{
    if (!g_motor_ctx.inited) {
        return MOTOR_RET_NOT_READY;
    }
    if (!motor_profile_valid(profile)) {
        return MOTOR_RET_INVALID_ARG;
    }

    g_motor_ctx.profile = profile;
    return MOTOR_RET_OK;
}

uint32_t motor_degree_to_steps(int32_t degree)
{
    uint32_t abs_deg = (degree < 0) ? (uint32_t)(-degree) : (uint32_t)degree;

    if (abs_deg == 0U) {
        return 0U;
    }
    if (abs_deg == 90U) {
        return MOTOR_STEPS_PER_90_DEG;
    }
    if (abs_deg == 180U) {
        return MOTOR_STEPS_PER_180_DEG;
    }
    if (abs_deg == 270U) {
        return MOTOR_STEPS_PER_270_DEG;
    }

    return (uint32_t)(((uint64_t)abs_deg * (uint64_t)MOTOR_STEPS_PER_90_DEG + 45ULL) / 90ULL);
}

motor_ret_t motor_turn(int32_t degree)
{
    uint32_t steps;
    uint8_t cw;

    if (!g_motor_ctx.inited) {
        return MOTOR_RET_NOT_READY;
    }

    if (degree == 0) {
        return MOTOR_RET_OK;
    }

    steps = motor_degree_to_steps(degree);
    if (steps == 0U) {
        return MOTOR_RET_INVALID_ARG;
    }

    cw = (degree > 0) ? 1U : 0U;
    motor_port_set_direction(cw);
    motor_port_set_enable(1U);

    if (motor_port_emit_steps(steps, g_motor_ctx.profile) != MOTOR_RET_OK) {
        motor_port_set_enable(0U);
        return MOTOR_RET_BUSY;
    }

    motor_port_set_enable(0U);
    return MOTOR_RET_OK;
}
