#if !defined(AT32F403Axx) && !defined(AT32F407xx)
#define AT32F403Axx
#endif

#if !defined(AT_START_F403A_V1) && !defined(AT_START_F407_V1)
#define AT_START_F403A_V1
#endif

#include "at32f403a_407_board.h"
#include "at32f403a_407_clock.h"
#include <stdint.h>
#include <stdbool.h>

#include "bsp_stepper.h"
#include "at32f403a_407_gpio.h"

#define BUTTON_PORT                        GPIOA
#define BUTTON_PIN                         GPIO_PINS_5
#define BUTTON_GPIO_CLK                    CRM_GPIOA_PERIPH_CLOCK
/* Hall sensors: M1=PB5, M2=PB9, M3=PB8. */
#define HALL_PORT                          GPIOB
#define HALL_GPIO_CLK                      CRM_GPIOB_PERIPH_CLOCK
#define HALL_M1_PIN                        GPIO_PINS_5
#define HALL_M2_PIN                        GPIO_PINS_9
#define HALL_M3_PIN                        GPIO_PINS_8

/* Motion tuning. M1/M2 are arms, M3 is the base/back cross. */
#define SPEED_ARM                          15000U
#define SPEED_BASE_TURN                    32000U
#define SPEED_BASE_SLOW                    6400U
#define BASE_OVERSHOOT_STEP                256
#define ARM_OVERSHOOT_STEP                 72
#define FLIP_OVERSHOOT_STEP                45

const int32_t HOMING_OFFSETS[3] = {140, 240, 110};

static bool hall_triggered(uint16_t pin)
{
    return ((HALL_PORT->idt & pin) == 0U);
}

static bool button_pressed(void)
{
    return ((BUTTON_PORT->idt & BUTTON_PIN) == 0U);
}

static void action_turn_arm(uint8_t motor_id, int32_t steps)
{
    int32_t dir_sign;

    dir_sign = (steps > 0) ? 1 : -1;

    motor_move_steps(motor_id, steps + (dir_sign * ARM_OVERSHOOT_STEP), SPEED_ARM);
    stepper_wait_all_done();

    motor_move_steps(motor_id, -dir_sign * ARM_OVERSHOOT_STEP, SPEED_ARM);
    stepper_wait_all_done();
}

static void action_turn_base(int32_t steps)
{
    int32_t dir_sign;
    int32_t abs_steps;

    dir_sign = (steps > 0) ? 1 : -1;
    abs_steps = (steps > 0) ? steps : -steps;

    /* 1. Large overshoot main rotation */
    motor_move_steps(STEPPER_M3_ID, dir_sign * (abs_steps + BASE_OVERSHOOT_STEP), SPEED_BASE_TURN);
    stepper_wait_all_done();

    /* 2. Slow return to precise target */
    motor_move_steps(STEPPER_M3_ID, -dir_sign * BASE_OVERSHOOT_STEP, SPEED_BASE_SLOW);
    stepper_wait_all_done();
}

static void action_flip_cube(int32_t steps)
{
    int32_t dir_sign;
    int32_t abs_steps;

    dir_sign = (steps > 0) ? 1 : -1;
    abs_steps = (steps > 0) ? steps : -steps;

    /* Left and right arms move in opposite directions for a cube flip. */
	motor_start_steps(STEPPER_M2_ID, -dir_sign * (abs_steps + FLIP_OVERSHOOT_STEP), SPEED_ARM);
	motor_start_steps(STEPPER_M1_ID, dir_sign * (abs_steps + FLIP_OVERSHOOT_STEP), SPEED_ARM);
    stepper_wait_all_done();

	motor_start_steps(STEPPER_M2_ID, dir_sign * FLIP_OVERSHOOT_STEP, SPEED_ARM);
	motor_start_steps(STEPPER_M1_ID, -dir_sign * FLIP_OVERSHOOT_STEP, SPEED_ARM);
    stepper_wait_all_done();
}

static void home_all_motors(void)
{
    const uint16_t home_speed = 2000U;
    const uint16_t offset_speed = 1000U;
    const uint16_t prehome_speed = 2000U;
    gpio_init_type gpio_init_struct;
    bool homed[STEPPER_MOTOR_COUNT] = { false, false, false };

    /* enable drivers */
    stepper_set_enable(1U);

    /* ensure GPIOB clock enabled for hall reads */
    crm_periph_clock_enable(HALL_GPIO_CLK, TRUE);
    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_pull = GPIO_PULL_UP;
    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
    gpio_init_struct.gpio_pins = HALL_M1_PIN | HALL_M2_PIN | HALL_M3_PIN;
    gpio_init(HALL_PORT, &gpio_init_struct);

    /* force all three axes to rotate +800 steps (90 degrees) before homing */
    motor_start_steps(STEPPER_M1_ID, +600, prehome_speed);
    motor_start_steps(STEPPER_M2_ID, +600, prehome_speed);
    motor_start_steps(STEPPER_M3_ID, +600, prehome_speed);
    stepper_wait_all_done();

    /* start all three axes moving toward home in clockwise direction (6400 steps) */
    motor_start_steps(STEPPER_M1_ID, +6400, home_speed);
    motor_start_steps(STEPPER_M2_ID, +6400, home_speed);
    motor_start_steps(STEPPER_M3_ID, +6400, home_speed);

    /* poll hall sensors; assume active low (RESET) when sensor triggered */
    while (!(homed[0] && homed[1] && homed[2])) {
        if (!homed[0]) {
            if (hall_triggered(HALL_M1_PIN)) {
                motor_stop(STEPPER_M1_ID);
                homed[0] = true;
                delay_ms(20);
            }
        }
        if (!homed[1]) {
            if (hall_triggered(HALL_M2_PIN)) {
                motor_stop(STEPPER_M2_ID);
                homed[1] = true;
                delay_ms(20);
            }
        }
        if (!homed[2]) {
            if (hall_triggered(HALL_M3_PIN)) {
                motor_stop(STEPPER_M3_ID);
                homed[2] = true;
                delay_ms(20);
            }
        }
    }

    /* apply homing offset compensation for all axes at the same time (in steps) */
    motor_start_steps(STEPPER_M1_ID, +HOMING_OFFSETS[0], offset_speed);
    motor_start_steps(STEPPER_M2_ID, +HOMING_OFFSETS[1], offset_speed);
    motor_start_steps(STEPPER_M3_ID, +HOMING_OFFSETS[2], offset_speed);
    stepper_wait_all_done();

    /* all homed; keep drivers enabled to hold torque until button press */
}

static void test_sequence_stage(void)
{
    stepper_set_enable(1U);

		// 50 steps cube scramble and restore test
	// 800 steps = 90 degree
	// 1600 steps = 180 degree
	// 2400 steps = 270 degree
	// M2 only positive
	// M3 positive / negative
	// M1 only negative
	// action_flip_cube(3200) = whole cube flip

	// ===== Step 1~25: scramble =====

	// 1. M2 +90
	action_turn_arm(STEPPER_M2_ID, 800);
	delay_ms(200);

	// 2. M3 +90
	action_turn_base(800);
	delay_ms(200);

	// 3. M1 -90
	action_turn_arm(STEPPER_M1_ID, -800);
	delay_ms(200);

	// 4. M2 +180
	action_turn_arm(STEPPER_M2_ID, 1600);
	delay_ms(200);

	// 5. Flip whole cube
	action_flip_cube(3200);
	delay_ms(1000);

	// 6. M3 -90
	action_turn_base(-800);
	delay_ms(200);

	// 7. M1 +90 equivalent to M1 -270
	action_turn_arm(STEPPER_M1_ID, -2400);
	delay_ms(200);

	// 8. M2 -90 equivalent to M2 +270
	action_turn_arm(STEPPER_M2_ID, 2400);
	delay_ms(200);

	// 9. M3 +180
	action_turn_base(1600);
	delay_ms(200);

	// 10. M1 +180 equivalent to M1 -180
	action_turn_arm(STEPPER_M1_ID, -1600);
	delay_ms(200);

	// 11. M2 +90
	action_turn_arm(STEPPER_M2_ID, 800);
	delay_ms(200);

	// 12. M3 +90
	action_turn_base(800);
	delay_ms(200);

	// 13. Flip whole cube
	action_flip_cube(3200);
	delay_ms(1000);

	// 14. M1 +180 equivalent to M1 -180
	action_turn_arm(STEPPER_M1_ID, -1600);
	delay_ms(200);

	// 15. M2 +180
	action_turn_arm(STEPPER_M2_ID, 1600);
	delay_ms(200);

	// 16. M3 -90
	action_turn_base(-800);
	delay_ms(200);

	// 17. M1 +90 equivalent to M1 -270
	action_turn_arm(STEPPER_M1_ID, -2400);
	delay_ms(200);

	// 18. M2 +90
	action_turn_arm(STEPPER_M2_ID, 800);
	delay_ms(200);

	// 19. M3 -180
	action_turn_base(-1600);
	delay_ms(200);

	// 20. Flip whole cube
	action_flip_cube(3200);
	delay_ms(1000);

	// 21. M1 -90
	action_turn_arm(STEPPER_M1_ID, -800);
	delay_ms(200);

	// 22. M2 +180
	action_turn_arm(STEPPER_M2_ID, 1600);
	delay_ms(200);

	// 23. M3 +90
	action_turn_base(800);
	delay_ms(200);

	// 24. M1 +90 equivalent to M1 -270
	action_turn_arm(STEPPER_M1_ID, -2400);
	delay_ms(200);

	// 25. M2 -90 equivalent to M2 +270
	action_turn_arm(STEPPER_M2_ID, 2400);
	delay_ms(200);


	// ===== Step 26~50: restore =====

	// 26. inverse of step 25: M2 +90
	action_turn_arm(STEPPER_M2_ID, 800);
	delay_ms(200);

	// 27. inverse of step 24: M1 -90
	action_turn_arm(STEPPER_M1_ID, -800);
	delay_ms(200);

	// 28. inverse of step 23: M3 -90
	action_turn_base(-800);
	delay_ms(200);

	// 29. inverse of step 22: M2 +180
	action_turn_arm(STEPPER_M2_ID, 1600);
	delay_ms(200);

	// 30. inverse of step 21: M1 +90 equivalent to M1 -270
	action_turn_arm(STEPPER_M1_ID, -2400);
	delay_ms(200);

	// 31. inverse of step 20: Flip whole cube
	action_flip_cube(3200);
	delay_ms(1000);

	// 32. inverse of step 19: M3 +180
	action_turn_base(1600);
	delay_ms(200);

	// 33. inverse of step 18: M2 -90 equivalent to M2 +270
	action_turn_arm(STEPPER_M2_ID, 2400);
	delay_ms(200);

	// 34. inverse of step 17: M1 -90
	action_turn_arm(STEPPER_M1_ID, -800);
	delay_ms(200);

	// 35. inverse of step 16: M3 +90
	action_turn_base(800);
	delay_ms(200);

	// 36. inverse of step 15: M2 +180
	action_turn_arm(STEPPER_M2_ID, 1600);
	delay_ms(200);

	// 37. inverse of step 14: M1 -180
	action_turn_arm(STEPPER_M1_ID, -1600);
	delay_ms(200);

	// 38. inverse of step 13: Flip whole cube
	action_flip_cube(3200);
	delay_ms(1000);

	// 39. inverse of step 12: M3 -90
	action_turn_base(-800);
	delay_ms(200);

	// 40. inverse of step 11: M2 -90 equivalent to M2 +270
	action_turn_arm(STEPPER_M2_ID, 2400);
	delay_ms(200);

	// 41. inverse of step 10: M1 -180
	action_turn_arm(STEPPER_M1_ID, -1600);
	delay_ms(200);

	// 42. inverse of step 9: M3 -180
	action_turn_base(-1600);
	delay_ms(200);

	// 43. inverse of step 8: M2 +90
	action_turn_arm(STEPPER_M2_ID, 800);
	delay_ms(200);

	// 44. inverse of step 7: M1 -90
	action_turn_arm(STEPPER_M1_ID, -800);
	delay_ms(200);

	// 45. inverse of step 6: M3 +90
	action_turn_base(800);
	delay_ms(200);

	// 46. inverse of step 5: Flip whole cube
	action_flip_cube(3200);
	delay_ms(1000);

	// 47. inverse of step 4: M2 +180
	action_turn_arm(STEPPER_M2_ID, 1600);
	delay_ms(200);

	// 48. inverse of step 3: M1 +90 equivalent to M1 -270
	action_turn_arm(STEPPER_M1_ID, -2400);
	delay_ms(200);

	// 49. inverse of step 2: M3 -90
	action_turn_base(-800);
	delay_ms(200);

	// 50. inverse of step 1: M2 -90 equivalent to M2 +270
	action_turn_arm(STEPPER_M2_ID, 2400);
	delay_ms(200);

    stepper_wait_all_done();
    delay_ms(200);

    /* keep drivers enabled to hold position */
}

static void button_init(void)
{
    gpio_init_type gpio_init_struct;

    crm_periph_clock_enable(BUTTON_GPIO_CLK, TRUE);

    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_pull = GPIO_PULL_UP;
    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
    gpio_init_struct.gpio_pins = BUTTON_PIN;
    gpio_init(BUTTON_PORT, &gpio_init_struct);
}

static void wait_for_button_press(void)
{
    /* wait until button pressed (active low) */
    while (!button_pressed()) {
        delay_ms(10);
    }

    /* debounce */
    delay_ms(50);

    /* wait for release */
    while (button_pressed()) {
        delay_ms(10);
    }

    delay_ms(50);
}

int main(void)
{
    system_clock_config();
    nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);
    delay_init();

    bsp_stepper_init();

    /* Stage 1: home all motors */
    home_all_motors();

    /* initialize button and wait for user press on PA5 joystick SW */
    button_init();
    wait_for_button_press();

    /* Stage: test sequence - each axis moves, then pause 1 second */
    test_sequence_stage();

    /* final: finished test sequence */

    while (1) {
    }
}

