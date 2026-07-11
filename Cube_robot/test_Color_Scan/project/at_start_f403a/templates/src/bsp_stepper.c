
#include "bsp_stepper.h"

#include "at32f403a_407_board.h"
#include "at32f403a_407_clock.h"
#include "at32f403a_407_gpio.h"

#include <math.h>
#include <stdbool.h>

typedef struct {
    tmr_type *tmr;
    tmr_channel_select_type channel;
    gpio_type *step_port;
    uint16_t step_pin;
    gpio_type *dir_port;
    uint16_t dir_pin;
    uint32_t tmr_clk;
    uint8_t tmr_irqn;
    uint8_t dir_cw_level;
    uint8_t dir_ccw_level;
} stepper_axis_cfg_t;

typedef struct {
    volatile uint8_t active;
    volatile uint32_t total_steps;
    volatile uint32_t done_steps;
    volatile uint32_t accel_steps;
    volatile uint32_t decel_begin_step;
    volatile float start_pps;
    volatile float target_pps;
    volatile float current_pps;
    volatile float accel_pps2;
} stepper_axis_state_t;

static const stepper_axis_cfg_t g_axis_cfg[STEPPER_MOTOR_COUNT] = {
    {STEPPER_M1_TMR, STEPPER_M1_TMR_CHANNEL, STEPPER_M1_STEP_PORT, STEPPER_M1_STEP_PIN, STEPPER_M1_DIR_PORT, STEPPER_M1_DIR_PIN, STEPPER_M1_TMR_CLK, STEPPER_M1_TMR_IRQn, STEPPER_M1_DIR_CW_LEVEL, STEPPER_M1_DIR_CCW_LEVEL},
    {STEPPER_M2_TMR, STEPPER_M2_TMR_CHANNEL, STEPPER_M2_STEP_PORT, STEPPER_M2_STEP_PIN, STEPPER_M2_DIR_PORT, STEPPER_M2_DIR_PIN, STEPPER_M2_TMR_CLK, STEPPER_M2_TMR_IRQn, STEPPER_M2_DIR_CW_LEVEL, STEPPER_M2_DIR_CCW_LEVEL},
    {STEPPER_M3_TMR, STEPPER_M3_TMR_CHANNEL, STEPPER_M3_STEP_PORT, STEPPER_M3_STEP_PIN, STEPPER_M3_DIR_PORT, STEPPER_M3_DIR_PIN, STEPPER_M3_TMR_CLK, STEPPER_M3_TMR_IRQn, STEPPER_M3_DIR_CW_LEVEL, STEPPER_M3_DIR_CCW_LEVEL}
};

static volatile stepper_axis_state_t g_axis_state[STEPPER_MOTOR_COUNT];

static uint32_t stepper_timer_clock_hz(const stepper_axis_cfg_t *cfg)
{
    crm_clocks_freq_type clocks;

    crm_clocks_freq_get(&clocks);
    if (cfg->tmr == TMR1 || cfg->tmr == TMR8 || cfg->tmr == TMR9) {
        return clocks.apb2_freq * 2U;
    }
    return clocks.apb1_freq * 2U;
}

static uint32_t stepper_speed_to_ticks(float pps)
{
    float clamped_pps = pps;
    uint32_t ticks;

    if (clamped_pps < STEPPER_MIN_SPEED_PPS) {
        clamped_pps = STEPPER_MIN_SPEED_PPS;
    }
    if (clamped_pps > STEPPER_MAX_SPEED_PPS) {
        clamped_pps = STEPPER_MAX_SPEED_PPS;
    }

    ticks = (uint32_t)((STEPPER_TIMER_TICK_HZ / clamped_pps) + 0.5f);
    if (ticks < 2U) {
        ticks = 2U;
    }
    if (ticks > STEPPER_TIMER_MAX_ARR) {
        ticks = STEPPER_TIMER_MAX_ARR;
    }

    return ticks;
}

static uint32_t stepper_degrees_to_steps(int degrees)
{
    uint32_t degree_abs;

    if (degrees == 0) {
        return 0U;
    }

    degree_abs = (degrees < 0) ? (uint32_t)(-degrees) : (uint32_t)degrees;
    return (uint32_t)(((uint64_t)degree_abs * (uint64_t)STEPPER_STEPS_PER_REV + 180ULL) / 360ULL);
}

static void stepper_set_axis_dir(uint8_t id, bool clockwise)
{
    const stepper_axis_cfg_t *cfg = &g_axis_cfg[id];
    uint8_t level = clockwise ? cfg->dir_cw_level : cfg->dir_ccw_level;

    gpio_bits_write(cfg->dir_port, cfg->dir_pin, (level != 0U) ? TRUE : FALSE);
}

static void stepper_apply_period(const stepper_axis_cfg_t *cfg, float pps)
{
    uint32_t ticks = stepper_speed_to_ticks(pps);

    tmr_period_value_set(cfg->tmr, ticks - 1U);
    tmr_channel_value_set(cfg->tmr, cfg->channel, ticks / 2U);
}

static void stepper_axis_stop(uint8_t id)
{
    const stepper_axis_cfg_t *cfg = &g_axis_cfg[id];

    tmr_channel_enable(cfg->tmr, cfg->channel, FALSE);
    tmr_counter_enable(cfg->tmr, FALSE);
    tmr_interrupt_enable(cfg->tmr, TMR_OVF_INT, FALSE);
    tmr_flag_clear(cfg->tmr, TMR_OVF_FLAG);
    g_axis_state[id].active = 0U;
}

void motor_stop(uint8_t id)
{
    if (id >= STEPPER_MOTOR_COUNT) {
        return;
    }

    stepper_axis_stop(id);
}

static void stepper_axis_update(uint8_t id)
{
    volatile stepper_axis_state_t *st = &g_axis_state[id];
    const stepper_axis_cfg_t *cfg = &g_axis_cfg[id];
    float next_pps;

    if (st->active == 0U) {
        return;
    }

    st->done_steps++;
    if (st->done_steps >= st->total_steps) {
        stepper_axis_stop(id);
        return;
    }

    next_pps = st->current_pps;
    if (st->done_steps < st->accel_steps) {
        next_pps = sqrtf((st->current_pps * st->current_pps) + (2.0f * st->accel_pps2));
        if (next_pps > st->target_pps) {
            next_pps = st->target_pps;
        }
    } else if (st->done_steps >= st->decel_begin_step) {
        float next_sq = (st->current_pps * st->current_pps) - (2.0f * st->accel_pps2);

        if (next_sq < (st->start_pps * st->start_pps)) {
            next_sq = st->start_pps * st->start_pps;
        }
        next_pps = sqrtf(next_sq);
    } else {
        next_pps = st->target_pps;
    }

    st->current_pps = next_pps;
    stepper_apply_period(cfg, next_pps);
}

static void stepper_axis_timer_init(const stepper_axis_cfg_t *cfg)
{
    tmr_output_config_type tmr_oc;
    uint16_t prescaler;
    uint32_t timer_clock_hz;

    timer_clock_hz = stepper_timer_clock_hz(cfg);
    prescaler = (uint16_t)((timer_clock_hz / STEPPER_TIMER_TICK_HZ) - 1U);

    tmr_base_init(cfg->tmr, STEPPER_TIMER_MAX_ARR, prescaler);
    tmr_cnt_dir_set(cfg->tmr, TMR_COUNT_UP);
    tmr_clock_source_div_set(cfg->tmr, TMR_CLOCK_DIV1);

    tmr_output_default_para_init(&tmr_oc);
    tmr_oc.oc_mode = TMR_OUTPUT_CONTROL_PWM_MODE_A;
    tmr_oc.oc_output_state = TRUE;
    tmr_oc.oc_idle_state = FALSE;
    tmr_oc.oc_polarity = TMR_OUTPUT_ACTIVE_HIGH;
    tmr_output_channel_config(cfg->tmr, cfg->channel, &tmr_oc);
    tmr_output_channel_buffer_enable(cfg->tmr, cfg->channel, TRUE);
    tmr_period_buffer_enable(cfg->tmr, TRUE);

    tmr_interrupt_enable(cfg->tmr, TMR_OVF_INT, FALSE);
    tmr_flag_clear(cfg->tmr, TMR_OVF_FLAG);
    tmr_channel_enable(cfg->tmr, cfg->channel, FALSE);
    tmr_counter_enable(cfg->tmr, FALSE);

    if (cfg->tmr == TMR1 || cfg->tmr == TMR8) {
        tmr_output_enable(cfg->tmr, TRUE);
    }
}

static void stepper_gpio_init(void)
{
    gpio_init_type gpio_init_struct;

    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(STEPPER_EN_GPIO_CLK, TRUE);
    crm_periph_clock_enable(STEPPER_M3_STEP_GPIO_CLK, TRUE);
    crm_periph_clock_enable(STEPPER_M3_DIR_GPIO_CLK, TRUE);
    crm_periph_clock_enable(STEPPER_M2_STEP_GPIO_CLK, TRUE);
    crm_periph_clock_enable(STEPPER_M2_DIR_GPIO_CLK, TRUE);
    crm_periph_clock_enable(STEPPER_M1_STEP_GPIO_CLK, TRUE);
    crm_periph_clock_enable(STEPPER_M1_DIR_GPIO_CLK, TRUE);
    gpio_pin_remap_config(TMR9_MUX, FALSE);

    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;

    gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_init_struct.gpio_pins = STEPPER_EN_PIN;
    gpio_init(STEPPER_EN_PORT, &gpio_init_struct);
    gpio_bits_write(STEPPER_EN_PORT, STEPPER_EN_PIN, TRUE);

    gpio_init_struct.gpio_pins = STEPPER_M3_DIR_PIN;
    gpio_init(STEPPER_M3_DIR_PORT, &gpio_init_struct);
    gpio_bits_write(STEPPER_M3_DIR_PORT, STEPPER_M3_DIR_PIN, (STEPPER_M3_DIR_CW_LEVEL != 0U) ? TRUE : FALSE);

    gpio_init_struct.gpio_pins = STEPPER_M2_DIR_PIN;
    gpio_init(STEPPER_M2_DIR_PORT, &gpio_init_struct);
    gpio_bits_write(STEPPER_M2_DIR_PORT, STEPPER_M2_DIR_PIN, (STEPPER_M2_DIR_CW_LEVEL != 0U) ? TRUE : FALSE);

    gpio_init_struct.gpio_pins = STEPPER_M1_DIR_PIN;
    gpio_init(STEPPER_M1_DIR_PORT, &gpio_init_struct);
    gpio_bits_write(STEPPER_M1_DIR_PORT, STEPPER_M1_DIR_PIN, (STEPPER_M1_DIR_CW_LEVEL != 0U) ? TRUE : FALSE);

    gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins = STEPPER_M3_STEP_PIN;
    gpio_init(STEPPER_M3_STEP_PORT, &gpio_init_struct);

    gpio_init_struct.gpio_pins = STEPPER_M2_STEP_PIN;
    gpio_init(STEPPER_M2_STEP_PORT, &gpio_init_struct);

    gpio_init_struct.gpio_pins = STEPPER_M1_STEP_PIN;
    gpio_init(STEPPER_M1_STEP_PORT, &gpio_init_struct);
}

void bsp_stepper_init(void)
{
    uint8_t i;

    stepper_gpio_init();

    crm_periph_clock_enable(STEPPER_M1_TMR_CLK, TRUE);
    crm_periph_clock_enable(STEPPER_M2_TMR_CLK, TRUE);
    crm_periph_clock_enable(STEPPER_M3_TMR_CLK, TRUE);

    stepper_axis_timer_init(&g_axis_cfg[STEPPER_M1_ID]);
    stepper_axis_timer_init(&g_axis_cfg[STEPPER_M2_ID]);
    stepper_axis_timer_init(&g_axis_cfg[STEPPER_M3_ID]);

    nvic_irq_enable(STEPPER_M1_TMR_IRQn, 1U, 0U);
    nvic_irq_enable(STEPPER_M2_TMR_IRQn, 1U, 0U);
    nvic_irq_enable(STEPPER_M3_TMR_IRQn, 1U, 0U);

    for (i = 0U; i < STEPPER_MOTOR_COUNT; i++) {
        g_axis_state[i].active = 0U;
        g_axis_state[i].total_steps = 0U;
        g_axis_state[i].done_steps = 0U;
        g_axis_state[i].accel_steps = 0U;
        g_axis_state[i].decel_begin_step = 0U;
        g_axis_state[i].start_pps = STEPPER_DEFAULT_START_PPS;
        g_axis_state[i].target_pps = STEPPER_DEFAULT_SPEED_PPS;
        g_axis_state[i].current_pps = STEPPER_DEFAULT_START_PPS;
        g_axis_state[i].accel_pps2 = STEPPER_DEFAULT_ACCEL_PPS2;
    }

    stepper_set_enable(0U);
}

void stepper_set_enable(uint8_t enable)
{
    uint8_t level = enable ? STEPPER_EN_ACTIVE_LEVEL : (uint8_t)(1U - STEPPER_EN_ACTIVE_LEVEL);

    gpio_bits_write(STEPPER_EN_PORT, STEPPER_EN_PIN, (level != 0U) ? TRUE : FALSE);
}

uint8_t stepper_is_busy(uint8_t id)
{
    if (id >= STEPPER_MOTOR_COUNT) {
        return 0U;
    }

    return g_axis_state[id].active;
}

uint32_t stepper_done_steps(uint8_t id)
{
    if (id >= STEPPER_MOTOR_COUNT) {
        return 0U;
    }

    return g_axis_state[id].done_steps;
}

void stepper_wait_all_done(void)
{
    while ((g_axis_state[STEPPER_M1_ID].active != 0U) ||
           (g_axis_state[STEPPER_M2_ID].active != 0U) ||
           (g_axis_state[STEPPER_M3_ID].active != 0U)) {
    }
}

static void stepper_axis_start(uint8_t id, uint32_t steps, bool clockwise, uint16_t speed)
{
    const stepper_axis_cfg_t *cfg;
    volatile stepper_axis_state_t *st;
    float requested_pps;
    float start_pps;
    float accel_steps_f;
    float peak_pps;
    uint32_t accel_steps;

    cfg = &g_axis_cfg[id];
    st = &g_axis_state[id];

    requested_pps = (speed == 0U) ? STEPPER_DEFAULT_SPEED_PPS : (float)speed;
    if (requested_pps < STEPPER_MIN_SPEED_PPS) {
        requested_pps = STEPPER_MIN_SPEED_PPS;
    }
    if (requested_pps > STEPPER_MAX_SPEED_PPS) {
        requested_pps = STEPPER_MAX_SPEED_PPS;
    }

    start_pps = STEPPER_DEFAULT_START_PPS;
    if (start_pps > requested_pps) {
        start_pps = requested_pps;
    }

    stepper_set_axis_dir(id, clockwise);

    st->total_steps = steps;
    st->done_steps = 0U;
    st->start_pps = start_pps;
    st->current_pps = start_pps;
    st->accel_pps2 = STEPPER_DEFAULT_ACCEL_PPS2;

    accel_steps_f = ((requested_pps * requested_pps) - (start_pps * start_pps)) / (2.0f * STEPPER_DEFAULT_ACCEL_PPS2);
    if (accel_steps_f < 1.0f) {
        accel_steps_f = 1.0f;
    }

    accel_steps = (uint32_t)ceilf(accel_steps_f);
    if ((accel_steps * 2U) > steps) {
        accel_steps = steps / 2U;
        if (accel_steps == 0U) {
            accel_steps = 1U;
        }
        peak_pps = sqrtf((start_pps * start_pps) + (2.0f * STEPPER_DEFAULT_ACCEL_PPS2 * (float)accel_steps));
        if (peak_pps < start_pps) {
            peak_pps = start_pps;
        }
        requested_pps = peak_pps;
    }

    st->accel_steps = accel_steps;
    st->decel_begin_step = steps - accel_steps;
    st->target_pps = requested_pps;

    tmr_counter_enable(cfg->tmr, FALSE);
    tmr_flag_clear(cfg->tmr, TMR_OVF_FLAG);
    stepper_apply_period(cfg, start_pps);

    st->active = 1U;
    tmr_interrupt_enable(cfg->tmr, TMR_OVF_INT, TRUE);
    tmr_channel_enable(cfg->tmr, cfg->channel, TRUE);
    tmr_counter_enable(cfg->tmr, TRUE);
}

void motor_move_degrees(uint8_t id, int degrees, uint16_t speed)
{
    uint32_t steps;
    bool clockwise;

    if (id >= STEPPER_MOTOR_COUNT) {
        return;
    }

    if (degrees == 0) {
        return;
    }

    while (g_axis_state[id].active != 0U) {
    }

    steps = stepper_degrees_to_steps(degrees);
    if (steps == 0U) {
        return;
    }

    clockwise = (degrees > 0);
    stepper_set_enable(1U);
    stepper_axis_start(id, steps, clockwise, speed);

    while (g_axis_state[id].active != 0U) {
    }
}

void motor_start_degrees(uint8_t id, int degrees, uint16_t speed)
{
    uint32_t steps;
    bool clockwise;

    if (id >= STEPPER_MOTOR_COUNT) {
        return;
    }

    if (degrees == 0) {
        return;
    }

    /* wait any prior move on this axis to finish */
    while (g_axis_state[id].active != 0U) {
    }

    steps = stepper_degrees_to_steps(degrees);
    if (steps == 0U) {
        return;
    }

    clockwise = (degrees > 0);
    stepper_set_enable(1U);
    stepper_axis_start(id, steps, clockwise, speed);
}

void motor_start_steps(uint8_t id, int32_t steps, uint16_t speed)
{
    bool clockwise;

    if (id >= STEPPER_MOTOR_COUNT) {
        return;
    }

    if (steps == 0) {
        return;
    }

    /* wait any prior move on this axis to finish */
    while (g_axis_state[id].active != 0U) {
    }

    clockwise = (steps > 0);
    stepper_set_enable(1U);
    stepper_axis_start(id, (uint32_t)((steps > 0) ? steps : -steps), clockwise, speed);
}

void motor_move_steps(uint8_t id, int32_t steps, uint16_t speed)
{
    bool clockwise;

    if (id >= STEPPER_MOTOR_COUNT) {
        return;
    }

    if (steps == 0) {
        return;
    }

    /* wait any prior move on this axis to finish */
    while (g_axis_state[id].active != 0U) {
    }

    clockwise = (steps > 0);
    stepper_set_enable(1U);
    stepper_axis_start(id, (uint32_t)((steps > 0) ? steps : -steps), clockwise, speed);

    /* blocking wait */
    while (g_axis_state[id].active != 0U) {
    }
}

void TMR3_GLOBAL_IRQHandler(void)
{
    if (tmr_interrupt_flag_get(TMR3, TMR_OVF_INT) != RESET) {
        tmr_flag_clear(TMR3, TMR_OVF_FLAG);
        stepper_axis_update(STEPPER_M1_ID);
    }
}

void TMR5_GLOBAL_IRQHandler(void)
{
    if (tmr_interrupt_flag_get(TMR5, TMR_OVF_INT) != RESET) {
        tmr_flag_clear(TMR5, TMR_OVF_FLAG);
        stepper_axis_update(STEPPER_M2_ID);
    }
}

void TMR1_BRK_TMR9_IRQHandler(void)
{
    if (tmr_interrupt_flag_get(TMR9, TMR_OVF_INT) != RESET) {
        tmr_flag_clear(TMR9, TMR_OVF_FLAG);
        stepper_axis_update(STEPPER_M3_ID);
    }
}

/* ------------------ High-level action wrappers ------------------ */
void action_turn_arm(int motor_id, int steps)
{
    uint8_t axis_id;
    int32_t cmd_steps;
    int32_t dir_sign;

    if (motor_id == STEPPER_M1_ID) {
        axis_id = STEPPER_M1_ID;
    } else if (motor_id == STEPPER_M2_ID) {
        axis_id = STEPPER_M2_ID;
    } else {
        return;
    }

    if (steps == 0) {
        return;
    }

    cmd_steps = steps;
    dir_sign = (cmd_steps > 0) ? 1 : -1;

    /* 1) main move + overshoot */
    motor_move_steps(axis_id, cmd_steps + (dir_sign * ACTION_ARM_OVERSHOOT_STEP), ACTION_SPEED_ARM_PPS);
    stepper_wait_all_done();

    /* 2) slow-ish compensation back */
    motor_move_steps(axis_id, -dir_sign * ACTION_ARM_OVERSHOOT_STEP, ACTION_SPEED_ARM_PPS);
    stepper_wait_all_done();
}

void action_turn_ud(int face_u_steps, int face_d_steps)
{
    const int32_t gap_steps = 24;
    int32_t min_steps;

    if (face_u_steps < 0) {
        face_u_steps = 0;
    }
    if (face_d_steps < 0) {
        face_d_steps = 0;
    }

    motor_start_steps(STEPPER_M1_ID, -gap_steps, ACTION_SPEED_ARM_PPS);
    motor_start_steps(STEPPER_M2_ID, +gap_steps, ACTION_SPEED_ARM_PPS);
    stepper_wait_all_done();

    min_steps = (face_u_steps < face_d_steps) ? face_u_steps : face_d_steps;
    if (min_steps > 0) {
        motor_start_steps(STEPPER_M1_ID, -min_steps, ACTION_SPEED_ARM_PPS);
        motor_start_steps(STEPPER_M2_ID, +min_steps, ACTION_SPEED_ARM_PPS);
        stepper_wait_all_done();
    }

    face_u_steps -= min_steps;
    face_d_steps -= min_steps;

    if (face_u_steps > 0) {
        motor_move_steps(STEPPER_M1_ID, -face_u_steps, ACTION_SPEED_ARM_PPS);
    }
    if (face_d_steps > 0) {
        motor_move_steps(STEPPER_M2_ID, +face_d_steps, ACTION_SPEED_ARM_PPS);
    }

    motor_start_steps(STEPPER_M1_ID, +gap_steps, ACTION_SPEED_ARM_PPS);
    motor_start_steps(STEPPER_M2_ID, -gap_steps, ACTION_SPEED_ARM_PPS);
    stepper_wait_all_done();
}

void action_turn_base(int steps)
{
    int32_t cmd_steps;
    int32_t dir_sign;
    int32_t abs_steps;

    if (steps == 0) {
        return;
    }

    cmd_steps = (int32_t)steps;
    dir_sign = (cmd_steps > 0) ? 1 : -1;
    abs_steps = (cmd_steps > 0) ? cmd_steps : -cmd_steps;

    motor_move_steps(STEPPER_M3_ID, dir_sign * ACTION_BASE_OVERSHOOT_STEP, ACTION_SPEED_BASE_TURN_PPS);

    if (abs_steps > ACTION_BASE_FINE_STEP) {
        motor_move_steps(STEPPER_M3_ID, dir_sign * (abs_steps - ACTION_BASE_FINE_STEP), ACTION_SPEED_BASE_TURN_PPS);
    }

    motor_move_steps(STEPPER_M3_ID, dir_sign * ACTION_BASE_FINE_STEP, ACTION_SPEED_BASE_SLOW_PPS);
    motor_move_steps(STEPPER_M3_ID, -dir_sign * ACTION_BASE_OVERSHOOT_STEP, ACTION_SPEED_BASE_TURN_PPS);
}

void action_flip_cube_steps(int steps)
{
    int32_t dir_sign;
    int32_t abs_steps;

    if (steps == 0) {
        return;
    }

    dir_sign = (steps > 0) ? 1 : -1;
    abs_steps = (steps > 0) ? steps : -steps;

    motor_start_steps(STEPPER_M2_ID, -dir_sign * (abs_steps + ACTION_FLIP_OVERSHOOT_STEP), ACTION_SPEED_ARM_PPS);
    motor_start_steps(STEPPER_M1_ID, dir_sign * (abs_steps + ACTION_FLIP_OVERSHOOT_STEP), ACTION_SPEED_ARM_PPS);
    stepper_wait_all_done();

    motor_start_steps(STEPPER_M2_ID, dir_sign * ACTION_FLIP_OVERSHOOT_STEP, ACTION_SPEED_ARM_PPS);
    motor_start_steps(STEPPER_M1_ID, -dir_sign * ACTION_FLIP_OVERSHOOT_STEP, ACTION_SPEED_ARM_PPS);
    stepper_wait_all_done();
}

void action_flip_cube(void)
{
    action_flip_cube_steps(ACTION_FLIP_DEFAULT_STEPS);
}
