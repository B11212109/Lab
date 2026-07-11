#include "motor_ctrl_port.h"
#include "motor_ctrl_cfg.h"

#include "at32f403a_407_board.h"
#include "at32f403a_407_conf.h"

#include <math.h>

static void motor_port_step_once(uint32_t period_us)
{
    uint32_t low_us = (period_us > MOTOR_STEP_HIGH_US) ? (period_us - MOTOR_STEP_HIGH_US) : MOTOR_STEP_GUARD_US;

    gpio_bits_write(MOTOR_STEP_PORT, MOTOR_STEP_PIN, SET);
    delay_us(MOTOR_STEP_HIGH_US);
    gpio_bits_write(MOTOR_STEP_PORT, MOTOR_STEP_PIN, RESET);
    delay_us((low_us > 0U) ? low_us : MOTOR_STEP_GUARD_US);
}

motor_ret_t motor_port_hw_init(void)
{
    gpio_init_type gpio_init_struct;

    crm_periph_clock_enable(MOTOR_EN_CRM_CLK, TRUE);
    crm_periph_clock_enable(MOTOR_DIR_CRM_CLK, TRUE);
    crm_periph_clock_enable(MOTOR_STEP_CRM_CLK, TRUE);

    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;

    gpio_init_struct.gpio_pins = MOTOR_EN_PIN;
    gpio_init(MOTOR_EN_PORT, &gpio_init_struct);

    gpio_init_struct.gpio_pins = MOTOR_DIR_PIN;
    gpio_init(MOTOR_DIR_PORT, &gpio_init_struct);

    gpio_init_struct.gpio_pins = MOTOR_STEP_PIN;
    gpio_init(MOTOR_STEP_PORT, &gpio_init_struct);

    motor_port_set_enable(0U);
    motor_port_set_direction(1U);
    gpio_bits_write(MOTOR_STEP_PORT, MOTOR_STEP_PIN, RESET);

    return MOTOR_RET_OK;
}

void motor_port_set_enable(uint8_t en)
{
    uint8_t level = en ? MOTOR_EN_ACTIVE_LEVEL : (uint8_t)(1U - MOTOR_EN_ACTIVE_LEVEL);
    gpio_bits_write(MOTOR_EN_PORT, MOTOR_EN_PIN, level ? SET : RESET);
}

void motor_port_set_direction(uint8_t cw)
{
    uint8_t level = cw ? MOTOR_DIR_CW_LEVEL : MOTOR_DIR_CCW_LEVEL;
    gpio_bits_write(MOTOR_DIR_PORT, MOTOR_DIR_PIN, level ? SET : RESET);
}

motor_ret_t motor_port_emit_steps(uint32_t steps, motor_profile_t profile)
{
    uint32_t i;
    uint32_t s;
    float vi;

    if (steps == 0U) {
        return MOTOR_RET_OK;
    }

    if (profile.v_start_pps <= 0.0f || profile.v_max_pps <= 0.0f || profile.accel_pps2 <= 0.0f) {
        return MOTOR_RET_INVALID_ARG;
    }

    s = (uint32_t)((profile.v_max_pps * profile.v_max_pps - profile.v_start_pps * profile.v_start_pps)
        / (2.0f * profile.accel_pps2));
    vi = profile.v_start_pps;

    for (i = 0U; i < steps; i++) {
        float accel;
        uint32_t period_us;

        if ((i < s) && (i < (steps / 2U))) {
            accel = profile.accel_pps2;
        } else if (i >= (steps - s)) {
            accel = -profile.accel_pps2;
        } else {
            accel = 0.0f;
        }

        vi = sqrtf(vi * vi + 2.0f * accel);
        if (vi < 1.0f) {
            vi = 1.0f;
        }

        period_us = (uint32_t)(1000000.0f / vi);
        if (period_us < (MOTOR_STEP_HIGH_US + MOTOR_STEP_GUARD_US)) {
            period_us = MOTOR_STEP_HIGH_US + MOTOR_STEP_GUARD_US;
        }
        motor_port_step_once(period_us);
    }

    return MOTOR_RET_OK;
}
