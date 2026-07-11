#ifndef BSP_STEPPER_H
#define BSP_STEPPER_H

#include <stdint.h>
#include "at32f403a_407.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STEPPER_MOTOR_COUNT                3U

#define STEPPER_M1_ID                      0U
#define STEPPER_M2_ID                      1U
#define STEPPER_M3_ID                      2U

/* Common EN pin: PA4, active low. */
#define STEPPER_EN_PORT                    GPIOA
#define STEPPER_EN_PIN                     GPIO_PINS_4
#define STEPPER_EN_GPIO_CLK                CRM_GPIOA_PERIPH_CLOCK
#define STEPPER_EN_ACTIVE_LEVEL            0U

/* Current axis mapping:
 * M1 = PA6/PA7/PB5
 * M2 = PA0/PA1/PB9
 * M3 = PA2/PA3/PB8
 */

/* M1: STEP=PA6, DIR=PA7, TMR3_CH1 */
#define STEPPER_M1_STEP_PORT               GPIOA
#define STEPPER_M1_STEP_PIN                GPIO_PINS_6
#define STEPPER_M1_STEP_GPIO_CLK           CRM_GPIOA_PERIPH_CLOCK
#define STEPPER_M1_DIR_PORT                GPIOA
#define STEPPER_M1_DIR_PIN                 GPIO_PINS_7
#define STEPPER_M1_DIR_GPIO_CLK            CRM_GPIOA_PERIPH_CLOCK
#define STEPPER_M1_TMR                     TMR3
#define STEPPER_M1_TMR_CLK                 CRM_TMR3_PERIPH_CLOCK
#define STEPPER_M1_TMR_IRQn                TMR3_GLOBAL_IRQn
#define STEPPER_M1_TMR_CHANNEL             TMR_SELECT_CHANNEL_1

/* M2: STEP=PA0, DIR=PA1, TMR5_CH1 */
#define STEPPER_M2_STEP_PORT               GPIOA
#define STEPPER_M2_STEP_PIN                GPIO_PINS_0
#define STEPPER_M2_STEP_GPIO_CLK           CRM_GPIOA_PERIPH_CLOCK
#define STEPPER_M2_DIR_PORT                GPIOA
#define STEPPER_M2_DIR_PIN                 GPIO_PINS_1
#define STEPPER_M2_DIR_GPIO_CLK            CRM_GPIOA_PERIPH_CLOCK
#define STEPPER_M2_TMR                     TMR5
#define STEPPER_M2_TMR_CLK                 CRM_TMR5_PERIPH_CLOCK
#define STEPPER_M2_TMR_IRQn                TMR5_GLOBAL_IRQn
#define STEPPER_M2_TMR_CHANNEL             TMR_SELECT_CHANNEL_1

/* M3: STEP=PA2, DIR=PA3, TMR9_CH1 */
#define STEPPER_M3_STEP_PORT               GPIOA
#define STEPPER_M3_STEP_PIN                GPIO_PINS_2
#define STEPPER_M3_STEP_GPIO_CLK           CRM_GPIOA_PERIPH_CLOCK
#define STEPPER_M3_DIR_PORT                GPIOA
#define STEPPER_M3_DIR_PIN                 GPIO_PINS_3
#define STEPPER_M3_DIR_GPIO_CLK            CRM_GPIOA_PERIPH_CLOCK
#define STEPPER_M3_TMR                     TMR9
#define STEPPER_M3_TMR_CLK                 CRM_TMR9_PERIPH_CLOCK
#define STEPPER_M3_TMR_IRQn                TMR1_BRK_TMR9_IRQn
#define STEPPER_M3_TMR_CHANNEL             TMR_SELECT_CHANNEL_1

/* Direction levels match test_Motor. */
#define STEPPER_M1_DIR_CW_LEVEL            1U
#define STEPPER_M1_DIR_CCW_LEVEL           0U
#define STEPPER_M2_DIR_CW_LEVEL            1U
#define STEPPER_M2_DIR_CCW_LEVEL           0U
#define STEPPER_M3_DIR_CW_LEVEL            1U
#define STEPPER_M3_DIR_CCW_LEVEL           0U

/* A4988 assumes 1/16 microstep on this board mapping. */
#define STEPPER_MOTOR_FULL_STEPS_PER_REV   200U
#define STEPPER_MICROSTEP_DIV              16U
#define STEPPER_STEPS_PER_REV              (STEPPER_MOTOR_FULL_STEPS_PER_REV * STEPPER_MICROSTEP_DIV)

/* Trapezoid profile defaults, in steps per second and steps per second^2. */
#define STEPPER_DEFAULT_START_PPS          300.0f
#define STEPPER_DEFAULT_ACCEL_PPS2         120000.0f
#define STEPPER_DEFAULT_SPEED_PPS          1600.0f
#define STEPPER_MAX_SPEED_PPS              32000.0f
#define STEPPER_MIN_SPEED_PPS              100.0f

/* Timer tick resolution used to convert pulse frequency to ARR/CCR values. */
#define STEPPER_TIMER_TICK_HZ              1000000U
#define STEPPER_TIMER_MAX_ARR              65535U

/* ===== action_* tuning params (from test_Motor main.c) ===== */
#define ACTION_SPEED_ARM_PPS               10000U
#define ACTION_SPEED_BASE_TURN_PPS         21333U
#define ACTION_SPEED_BASE_SLOW_PPS         4267U
#define ACTION_BASE_OVERSHOOT_STEP         256
#define ACTION_BASE_FINE_STEP              64
#define ACTION_ARM_OVERSHOOT_STEP          135
#define ACTION_FLIP_OVERSHOOT_STEP         90
#define ACTION_FLIP_DEFAULT_STEPS          3200

void bsp_stepper_init(void);
void stepper_set_enable(uint8_t enable);
void motor_move_degrees(uint8_t id, int degrees, uint16_t speed);
/* Start a move without blocking; use `stepper_is_busy()` or `stepper_wait_all_done()` to poll */
void motor_start_degrees(uint8_t id, int degrees, uint16_t speed);
/* Stop an axis immediately */
void motor_stop(uint8_t id);
uint8_t stepper_is_busy(uint8_t id);
uint32_t stepper_done_steps(uint8_t id);
void stepper_wait_all_done(void);
/* Step-based motor control API */
void motor_move_steps(uint8_t id, int32_t steps, uint16_t speed);
void motor_start_steps(uint8_t id, int32_t steps, uint16_t speed);

/* High-level action wrappers. */
void action_turn_arm(int motor_id, int steps);
void action_turn_ud(int face_u_steps, int face_d_steps);
void action_turn_base(int steps);
void action_flip_cube_steps(int steps);
void action_flip_cube(void);

#ifdef __cplusplus
}
#endif

#endif
