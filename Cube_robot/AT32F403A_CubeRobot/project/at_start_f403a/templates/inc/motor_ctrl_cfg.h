#ifndef MOTOR_CTRL_CFG_H
#define MOTOR_CTRL_CFG_H

#include "at32f403a_407.h"

/* Pin mapping macros: modify here only, do not hardcode in source files. */
#define MOTOR_EN_PORT                   GPIOA
#define MOTOR_EN_PIN                    GPIO_PINS_4
#define MOTOR_EN_CRM_CLK                CRM_GPIOA_PERIPH_CLOCK

#define MOTOR_DIR_PORT                  GPIOA
#define MOTOR_DIR_PIN                   GPIO_PINS_1
#define MOTOR_DIR_CRM_CLK               CRM_GPIOA_PERIPH_CLOCK

#define MOTOR_STEP_PORT                 GPIOA
#define MOTOR_STEP_PIN                  GPIO_PINS_0
#define MOTOR_STEP_CRM_CLK              CRM_GPIOA_PERIPH_CLOCK

/* Timer mapping for pulse generation. */
#define MOTOR_TMR                       TMR5
#define MOTOR_TMR_CRM_CLK               CRM_TMR5_PERIPH_CLOCK
#define MOTOR_TMR_IRQn                  TMR5_GLOBAL_IRQn

/* A4988 EN is active-low; DIR level follows v3 motor0 convention. */
#define MOTOR_EN_ACTIVE_LEVEL           0U
#define MOTOR_DIR_CW_LEVEL              1U
#define MOTOR_DIR_CCW_LEVEL             0U

/* STEP pulse timing constraints (A4988 min high/low >= 1us). */
#define MOTOR_STEP_HIGH_US              3U
#define MOTOR_STEP_GUARD_US             1U

/* Mechanical/electrical conversion defaults from RP2040 logic baseline. */
#define MOTOR_STEPS_PER_90_DEG          800U
#define MOTOR_STEPS_PER_180_DEG         1600U
#define MOTOR_STEPS_PER_270_DEG         2400U

/* Speed profile defaults (pulse/s, pulse/s^2). */
#define MOTOR_V_START_PPS               500.0f
#define MOTOR_V_MAX_PPS                 5000.0f
#define MOTOR_ACCEL_PPS2                500000.0f

#endif
