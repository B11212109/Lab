/**
  ***************************************************************************
  * @file     main.c
  * @brief    AT32 cube robot main application
  *
  * Flow follows the original Pico project:
  *   scan colors -> build 54-face string -> solve tables -> execute moves.
  *
 * UART and status LED are intentionally not used here.
 * The OLED joystick UI is the normal entry point.
  ***************************************************************************
  */

#if !defined(AT32F403Axx) && !defined(AT32F407xx)
#define AT32F403Axx
#endif

#if !defined(AT_START_F403A_V1) && !defined(AT_START_F407_V1)
#define AT_START_F403A_V1
#endif

#include "at32f403a_407_board.h"
#include "at32f403a_407_clock.h"
#include "at32f403a_407_gpio.h"
#include "bsp_hmi.h"
#include "bsp_stepper.h"
#include "color_detect.h"
#include "color_sensor_api.h"
#include "cube_app.h"
#include "scramble_table.h"
#include "solve.h"
#include "ui_menu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* PA11 is used by OLED reset; PC6 is unused in this hardware revision. */
#define SYSTEM_BUTTON_ENABLED              0U
#define BUTTON_PORT                        GPIOA
#define BUTTON_PIN                         GPIO_PINS_5
#define BUTTON_GPIO_CLK                    CRM_GPIOA_PERIPH_CLOCK

/* Hall sensors for homing: PB5/PB9/PB8 */
#define HALL_PORT                          GPIOB
#define HALL_GPIO_CLK                      CRM_GPIOB_PERIPH_CLOCK
#define HALL_M1_PIN                        GPIO_PINS_5
#define HALL_M2_PIN                        GPIO_PINS_9
#define HALL_M3_PIN                        GPIO_PINS_8

/* ---------------- Motion tuning ---------------- */
#define HOMING_PREMOVE_STEPS               600
#define HOMING_SEARCH_STEPS                6400
#define HOMING_OFFSETS_M1                  300
#define HOMING_OFFSETS_M2                  265
#define HOMING_OFFSETS_M3                  105

#define CUBE_FACE_LEN                      54U
#define SOLVE_STAGE_COUNT                  4U
#define SCAN_STEP_UNIT                     400U
#define SCAN_RGB_CHANNELS                  3U
#define PATTERN_MAX_MOVES                  64U
#define COLOR_SENSOR_SWAP_LR               1U
#define UD(x, y)                           ((((uint32_t)(x)) << 8) | (uint32_t)(y))
/* UI speed 150% is the author/test_Motor speed; 100% is the user's normal speed. */
#define MOTION_UI_AUTHOR_SPEED_PERCENT     150U
#define MOTION_UI_BASE_FRAME_DELAY_MS      40U
#define MOTION_UI_MIN_FRAME_DELAY_MS       20U
#define MOTION_UI_MAX_FRAME_DELAY_MS       120U

typedef struct {
    const char *name;
    const char *formula;
} pattern_entry_t;

typedef enum {
    MOTION_UI_NONE = 0,
    MOTION_UI_TASK,
    MOTION_UI_SCRAMBLE,
    MOTION_UI_PATTERN
} motion_ui_mode_t;

static const pattern_entry_t g_pattern_table[UI_PATTERN_COUNT] = {
    {"Checker", "L2 R2 F2 B2 U2 D2 "},
    {"Box", "U' D F' B L R' U' D "},
    {"Cross", "B2 F' L2 R2 D2 B2 F2 L2 R2 U2 F' "},
    {"Slash", "B L2 U2 L2 B' F' U2 R' B F R2 D' L R' D' U R F' "},
    {"Cube3", "F D2 L2 B D B' F2 U' F U F2 U2 F' L D F' U "},
    {"Fish", "L2 D B2 U R2 B2 D L' B2 F' D' U R' D2 R' B2 F' U' F' "}
};

static char cube_str[CUBE_FACE_LEN + 1U] = {0};
static char solution_str[SOLVE_STAGE_COUNT][55] = {{0}};
static uint16_t color_buffer[CUBE_FACE_LEN * SCAN_RGB_CHANNELS] = {0};
static char g_face_on_stepper_2 = 'F';
static bool g_color_ready = false;
static uint8_t g_scramble_table_index = 0U;
static uint8_t g_last_scramble_table_index = 0U;
static uint16_t g_last_scramble_steps = 20U;
static uint16_t g_scramble_run_count = 0U;
static motion_ui_mode_t g_motion_ui_mode = MOTION_UI_NONE;
static char g_motion_title[16] = {0};
static char g_motion_status[16] = {0};
static uint16_t g_motion_remaining_steps = 0U;
static uint8_t g_motion_percent = 0U;
static uint16_t g_motion_frame = 0U;

static bool capture_color_pair(uint32_t cmd);
static bool scan_arm_pair(int32_t steps, const uint32_t *color_cmd, size_t color_cmd_count);
static void store_color_sample(uint8_t index, const color_rgb_t *rgb);
static void motion_ui_set_task(const char *title, uint8_t percent, const char *status);
static void motion_ui_set_scramble(uint16_t remaining_steps, uint8_t percent, const char *status);
static void motion_ui_set_pattern(const char *title, uint8_t percent);
static void motion_ui_disable(void);
static uint16_t motion_ui_frame_delay_ms(void);
static void motion_ui_service(void);
static bool emergency_stop_requested(void);
static bool emergency_stop_poll(void);
static void emergency_stop_now(void);
static void emergency_stop_return_to_main(void);
static void emergency_stop_clear_for_next_action(void);
static uint8_t solve_abort_poll(void);
static bool scramble_table_read_move(const char **cursor, char *move);
static uint16_t count_solution_moves(const char *solution);
static uint16_t parse_solution_moves(const char *solution, char moves[][3], uint16_t max_moves);
static void execute_pattern_moves(const char *title, char moves[][3],
                                  uint16_t total_moves, uint8_t reverse);
static void align_cube_for_scan(void);
static void run_solve_cycle(uint8_t run_prepare);
static ui_scramble_done_action_t run_scramble_cycle(uint16_t steps);
static void run_scramble_menu_flow(uint16_t steps);
static void run_pattern_cycle(ui_pattern_t pattern);

static bool hall_triggered(uint16_t pin)
{
    return ((HALL_PORT->idt & pin) == 0U);
}

static bool button_pressed(void)
{
#if SYSTEM_BUTTON_ENABLED
    return ((BUTTON_PORT->idt & BUTTON_PIN) == 0U);
#else
    return false;
#endif
}

static void motion_ui_copy_text(char *dst, size_t dst_size, const char *src)
{
    size_t i = 0U;

    if (dst == NULL || dst_size == 0U) {
        return;
    }
    if (src == NULL) {
        src = "";
    }

    while (src[i] != '\0' && (i + 1U) < dst_size) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void motion_ui_set_task(const char *title, uint8_t percent, const char *status)
{
    motion_ui_copy_text(g_motion_title, sizeof(g_motion_title), title);
    motion_ui_copy_text(g_motion_status, sizeof(g_motion_status), status);
    g_motion_percent = (percent > 100U) ? 100U : percent;
    g_motion_ui_mode = MOTION_UI_TASK;
}

static void motion_ui_set_scramble(uint16_t remaining_steps, uint8_t percent, const char *status)
{
    motion_ui_copy_text(g_motion_status, sizeof(g_motion_status), status);
    g_motion_remaining_steps = remaining_steps;
    g_motion_percent = (percent > 100U) ? 100U : percent;
    g_motion_ui_mode = MOTION_UI_SCRAMBLE;
}

static void motion_ui_set_pattern(const char *title, uint8_t percent)
{
    motion_ui_copy_text(g_motion_title, sizeof(g_motion_title), title);
    g_motion_percent = (percent > 100U) ? 100U : percent;
    g_motion_ui_mode = MOTION_UI_PATTERN;
}

static void motion_ui_disable(void)
{
    g_motion_ui_mode = MOTION_UI_NONE;
}

static bool emergency_stop_requested(void)
{
    return (ui_menu_is_emergency_stop_requested() != 0U ||
            stepper_emergency_stop_requested() != 0U);
}

static void emergency_stop_now(void)
{
    stepper_emergency_stop();
    motion_ui_disable();
    ui_menu_request_emergency_stop();
}

static bool emergency_stop_poll(void)
{
    if (hmi_consume_reset_button() != 0U) {
        emergency_stop_now();
        return true;
    }

    if (emergency_stop_requested()) {
        stepper_emergency_stop();
        motion_ui_disable();
        return true;
    }

    return false;
}

static uint8_t solve_abort_poll(void)
{
    return emergency_stop_poll() ? 1U : 0U;
}

static void emergency_stop_return_to_main(void)
{
    stepper_emergency_stop();
    motion_ui_disable();
    ui_menu_show_main();
}

static void emergency_stop_clear_for_next_action(void)
{
    ui_menu_clear_emergency_stop();
    stepper_clear_emergency_stop();
}

static uint16_t motion_ui_frame_delay_ms(void)
{
    uint32_t delay_ms_value;
    uint16_t speed_percent = stepper_get_speed_percent();

    if (speed_percent < 50U) {
        speed_percent = 50U;
    }
    if (speed_percent > MOTION_UI_AUTHOR_SPEED_PERCENT) {
        speed_percent = MOTION_UI_AUTHOR_SPEED_PERCENT;
    }

    delay_ms_value = ((uint32_t)MOTION_UI_BASE_FRAME_DELAY_MS *
                      (uint32_t)MOTION_UI_AUTHOR_SPEED_PERCENT) / speed_percent;
    if (delay_ms_value < MOTION_UI_MIN_FRAME_DELAY_MS) {
        delay_ms_value = MOTION_UI_MIN_FRAME_DELAY_MS;
    }
    if (delay_ms_value > MOTION_UI_MAX_FRAME_DELAY_MS) {
        delay_ms_value = MOTION_UI_MAX_FRAME_DELAY_MS;
    }

    return (uint16_t)delay_ms_value;
}

static void motion_ui_service(void)
{
    if (emergency_stop_poll()) {
        delay_ms(1);
        return;
    }

    if (g_motion_ui_mode == MOTION_UI_NONE) {
        delay_ms(1);
        return;
    }

    g_motion_frame++;
    if (g_motion_ui_mode == MOTION_UI_TASK) {
        ui_menu_show_task_frame(g_motion_title,
                                g_motion_percent,
                                g_motion_status,
                                g_motion_frame);
    } else if (g_motion_ui_mode == MOTION_UI_SCRAMBLE) {
        ui_menu_show_scramble_step_frame(g_motion_remaining_steps,
                                         g_motion_percent,
                                         g_motion_status,
                                         g_motion_frame);
    } else if (g_motion_ui_mode == MOTION_UI_PATTERN) {
        ui_menu_show_pattern_frame(g_motion_title,
                                   g_motion_percent,
                                   g_motion_frame);
    }

    delay_ms(motion_ui_frame_delay_ms());
}

static void io_init(void)
{
    gpio_init_type gpio_init_struct;

#if SYSTEM_BUTTON_ENABLED
    crm_periph_clock_enable(BUTTON_GPIO_CLK, TRUE);
#endif
    crm_periph_clock_enable(HALL_GPIO_CLK, TRUE);

    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_pull = GPIO_PULL_UP;
    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;

#if SYSTEM_BUTTON_ENABLED
    gpio_init_struct.gpio_pins = BUTTON_PIN;
    gpio_init(BUTTON_PORT, &gpio_init_struct);
#endif

    gpio_init_struct.gpio_pins = HALL_M1_PIN | HALL_M2_PIN | HALL_M3_PIN;
    gpio_init(HALL_PORT, &gpio_init_struct);
}

static void home_all_motors(void)
{
    const uint16_t home_speed = 2000U;
    const uint16_t offset_speed = 1000U;
    const uint16_t prehome_speed = 2000U;
    bool homed[STEPPER_MOTOR_COUNT] = { false, false, false };

    stepper_set_enable(1U);

    motor_start_steps(STEPPER_M1_ID, +HOMING_PREMOVE_STEPS, prehome_speed);
    motor_start_steps(STEPPER_M2_ID, +HOMING_PREMOVE_STEPS, prehome_speed);
    motor_start_steps(STEPPER_M3_ID, +HOMING_PREMOVE_STEPS, prehome_speed);
    stepper_wait_all_done();
    if (emergency_stop_requested()) {
        return;
    }

    motor_start_steps(STEPPER_M1_ID, +HOMING_SEARCH_STEPS, home_speed);
    motor_start_steps(STEPPER_M2_ID, +HOMING_SEARCH_STEPS, home_speed);
    motor_start_steps(STEPPER_M3_ID, +HOMING_SEARCH_STEPS, home_speed);

    while (!(homed[0] && homed[1] && homed[2])) {
        if (emergency_stop_poll()) {
            return;
        }
        if (!homed[0] && hall_triggered(HALL_M1_PIN)) {
            motor_stop(STEPPER_M1_ID);
            homed[0] = true;
            delay_ms(20);
        }
        if (!homed[1] && hall_triggered(HALL_M2_PIN)) {
            motor_stop(STEPPER_M2_ID);
            homed[1] = true;
            delay_ms(20);
        }
        if (!homed[2] && hall_triggered(HALL_M3_PIN)) {
            motor_stop(STEPPER_M3_ID);
            homed[2] = true;
            delay_ms(20);
        }
        delay_ms(1);
    }

    motor_start_steps(STEPPER_M1_ID, +HOMING_OFFSETS_M1, offset_speed);
    motor_start_steps(STEPPER_M2_ID, +HOMING_OFFSETS_M2, offset_speed);
    motor_start_steps(STEPPER_M3_ID, +HOMING_OFFSETS_M3, offset_speed);
    stepper_wait_all_done();
}

static bool cube_scan_face_54(char *out_face_54, size_t out_size)
{
    static const uint32_t color_cmd_0_270_L[7] = {UD(36, 42), UD(23, 48), UD(38, 44), UD(10, 43), UD(9, 15), UD(50, 21), UD(11, 17)};
    static const uint32_t color_cmd_1_180_R[5] = {0U, UD(37, 16), UD(33, 27), UD(19, 52), UD(0, 6)};
    static const uint32_t color_cmd_2_270_F[7] = {0U, 0U, UD(29, 35), UD(46, 25), UD(8, 2), 0U, 0U};
    static const uint32_t color_cmd_3_180_B[5] = {0U, 0U, UD(18, 51), UD(12, 41), UD(47, 26)};
    static const uint32_t color_cmd_4_270_R[7] = {0U, 0U, UD(45, 24), UD(39, 14), UD(20, 53), 0U, 0U};
    static const uint32_t color_cmd_5_180_L[5] = {0U, 0U, 0U, UD(5, 32), 0U};
    static const uint32_t color_cmd_7_180_B[5] = {0U, UD(3, 30), 0U, UD(7, 28), 0U};
    static const uint32_t color_cmd_8_180[5] = {0U, 0U, 0U, UD(1, 34), 0U};

    if (!g_color_ready || out_face_54 == NULL || out_size < (CUBE_FACE_LEN + 1U)) {
        if (out_face_54 != NULL && out_size > 0U) {
            out_face_54[0] = '\0';
        }
        return false;
    }

    memset(color_buffer, 0, sizeof(color_buffer));

    ui_menu_show_task("SOLVE", 5U, "SCAN");
    if (!scan_arm_pair(+2400, color_cmd_0_270_L, sizeof(color_cmd_0_270_L) / sizeof(color_cmd_0_270_L[0]))) return false;
    if (emergency_stop_requested()) return false;
    action_turn_base(800);
    if (emergency_stop_requested()) return false;
    ui_menu_show_task("SOLVE", 10U, "SCAN");
    if (!scan_arm_pair(+1600, color_cmd_1_180_R, sizeof(color_cmd_1_180_R) / sizeof(color_cmd_1_180_R[0]))) return false;
    if (emergency_stop_requested()) return false;
    action_turn_base(800);
    if (emergency_stop_requested()) return false;
    ui_menu_show_task("SOLVE", 15U, "SCAN");
    if (!scan_arm_pair(+2400, color_cmd_2_270_F, sizeof(color_cmd_2_270_F) / sizeof(color_cmd_2_270_F[0]))) return false;
    if (emergency_stop_requested()) return false;
    action_turn_base(800);
    if (emergency_stop_requested()) return false;
    ui_menu_show_task("SOLVE", 20U, "SCAN");
    if (!scan_arm_pair(+1600, color_cmd_3_180_B, sizeof(color_cmd_3_180_B) / sizeof(color_cmd_3_180_B[0]))) return false;
    if (emergency_stop_requested()) return false;
    action_turn_base(800);
    if (emergency_stop_requested()) return false;
    ui_menu_show_task("SOLVE", 25U, "SCAN");
    if (!scan_arm_pair(+2400, color_cmd_4_270_R, sizeof(color_cmd_4_270_R) / sizeof(color_cmd_4_270_R[0]))) return false;
    if (emergency_stop_requested()) return false;
    action_turn_base(800);
    if (emergency_stop_requested()) return false;
    ui_menu_show_task("SOLVE", 30U, "SCAN");
    if (!scan_arm_pair(+1600, color_cmd_5_180_L, sizeof(color_cmd_5_180_L) / sizeof(color_cmd_5_180_L[0]))) return false;
    if (emergency_stop_requested()) return false;
    action_turn_base(800);
    if (emergency_stop_requested()) return false;
    ui_menu_show_task("SOLVE", 35U, "SCAN");
    if (!scan_arm_pair(+800, NULL, 0U)) return false;
    if (emergency_stop_requested()) return false;
    action_turn_base(800);
    if (emergency_stop_requested()) return false;
    if (!scan_arm_pair(+1600, color_cmd_7_180_B, sizeof(color_cmd_7_180_B) / sizeof(color_cmd_7_180_B[0]))) return false;
    if (emergency_stop_requested()) return false;
    action_turn_base(800);
    if (emergency_stop_requested()) return false;
    ui_menu_show_task("SOLVE", 40U, "SCAN");
    if (!scan_arm_pair(+1600, color_cmd_8_180, sizeof(color_cmd_8_180) / sizeof(color_cmd_8_180[0]))) return false;
    if (emergency_stop_requested()) return false;

    color_detect(color_buffer, out_face_54);
    return (out_face_54[0] != '\0');
}

static bool capture_color_pair(uint32_t cmd)
{
    color_rgb_t sensor0_rgb;
    color_rgb_t sensor1_rgb;
    uint8_t left_index;
    uint8_t right_index;

    if (cmd == 0U) {
        return true;
    }

    left_index = (uint8_t)((cmd >> 8) & 0xFFU);
    right_index = (uint8_t)(cmd & 0xFFU);
    if (left_index >= CUBE_FACE_LEN || right_index >= CUBE_FACE_LEN) {
        return false;
    }

    if (color_sensor_read_rgb_by_id(0U, &sensor0_rgb) != COLOR_RET_OK) {
        return false;
    }
    if (color_sensor_read_rgb_by_id(1U, &sensor1_rgb) != COLOR_RET_OK) {
        return false;
    }

#if COLOR_SENSOR_SWAP_LR
    store_color_sample(left_index, &sensor1_rgb);
    store_color_sample(right_index, &sensor0_rgb);
#else
    store_color_sample(left_index, &sensor0_rgb);
    store_color_sample(right_index, &sensor1_rgb);
#endif

    return true;
}

static void store_color_sample(uint8_t index, const color_rgb_t *rgb)
{
    color_buffer[index * SCAN_RGB_CHANNELS + 0U] = rgb->r;
    color_buffer[index * SCAN_RGB_CHANNELS + 1U] = rgb->g;
    color_buffer[index * SCAN_RGB_CHANNELS + 2U] = rgb->b;
}

static bool scan_arm_pair(int32_t steps, const uint32_t *color_cmd, size_t color_cmd_count)
{
    uint32_t total_steps;
    uint32_t next_sample;
    int32_t dir_sign;

    if (steps == 0) {
        return true;
    }

    dir_sign = (steps > 0) ? 1 : -1;
    total_steps = (uint32_t)((steps > 0) ? steps : -steps);

    motor_start_steps(STEPPER_M1_ID, dir_sign * (int32_t)total_steps, ACTION_SPEED_ARM_PPS);
    motor_start_steps(STEPPER_M2_ID, -dir_sign * (int32_t)total_steps, ACTION_SPEED_ARM_PPS);

    next_sample = 0U;
    while (stepper_is_busy(STEPPER_M1_ID) || stepper_is_busy(STEPPER_M2_ID)) {
        if (emergency_stop_poll()) {
            return false;
        }
        if ((color_cmd != NULL) && (next_sample < color_cmd_count)) {
            uint32_t threshold = (next_sample == 0U) ? 1U : (next_sample * SCAN_STEP_UNIT);
            if (stepper_done_steps(STEPPER_M1_ID) >= threshold) {
                if (!capture_color_pair(color_cmd[next_sample])) {
                    motor_stop(STEPPER_M1_ID);
                    motor_stop(STEPPER_M2_ID);
                    return false;
                }
                next_sample++;
            }
        }
        delay_ms(1);
    }

    while ((color_cmd != NULL) && (next_sample < color_cmd_count)) {
        if (emergency_stop_requested()) {
            return false;
        }
        if (!capture_color_pair(color_cmd[next_sample])) {
            return false;
        }
        next_sample++;
    }

    return true;
}

static void solve_and_execute(const char *face_54)
{
    cube_t cube;
    unsigned int stage;
    uint8_t solve_percent;
    uint8_t move_percent;

    cube_from_face_54(&cube, face_54);

    for (stage = 0U; stage < SOLVE_STAGE_COUNT; ++stage) {
        char *p = solution_str[stage];
        if (emergency_stop_requested()) {
            return;
        }
        memset(solution_str[stage], 0, sizeof(solution_str[stage]));
        solve_percent = (uint8_t)(45U + (stage * 10U));
        move_percent = (uint8_t)(55U + (stage * 10U));
        ui_menu_show_task("SOLVE", solve_percent, "SEARCH");
        p = solve((int)stage, &cube, p);
        (void)p;
        if (solve_last_aborted() != 0U) {
            return;
        }
        if (emergency_stop_requested()) {
            return;
        }

        ui_menu_show_task("SOLVE", move_percent, "MOVE");
        motion_ui_set_task("SOLVE", move_percent, "MOVE");
        g_face_on_stepper_2 = cube_app_execute_solution(g_face_on_stepper_2,
                                                        solution_str[stage]);
        motion_ui_disable();
        if (emergency_stop_requested()) {
            return;
        }
    }
}

static void prepare_cube_for_menu_action(void)
{
    ui_menu_show_homing_progress();
    if (emergency_stop_requested()) {
        return;
    }
    home_all_motors();
    if (emergency_stop_requested()) {
        return;
    }
    ui_menu_wait_place_cube();
    if (!emergency_stop_requested()) {
        g_face_on_stepper_2 = 'F';
    }
}

static void align_cube_for_scan(void)
{
    if (g_face_on_stepper_2 == 'F') {
        return;
    }

    ui_menu_show_task("SOLVE", 0U, "ALIGN");
    motion_ui_set_task("SOLVE", 0U, "ALIGN");
    g_face_on_stepper_2 = cube_app_execute_solution(g_face_on_stepper_2, "F0");
    motion_ui_disable();
}

static void run_solve_cycle(uint8_t run_prepare)
{
    if (run_prepare != 0U) {
        prepare_cube_for_menu_action();
        if (emergency_stop_requested()) {
            ui_menu_show_main();
            return;
        }
    } else {
        align_cube_for_scan();
        if (emergency_stop_requested()) {
            ui_menu_show_main();
            return;
        }
    }

    ui_menu_show_task("SOLVE", 0U, "START");

    if (!cube_scan_face_54(cube_str, sizeof(cube_str))) {
        if (emergency_stop_requested()) {
            ui_menu_show_main();
            return;
        }
        ui_menu_show_task("SOLVE", 0U, "SCAN FAIL");
        delay_ms(700);
        ui_menu_show_main();
        return;
    }

    if (cube_str[0] == '\0') {
        ui_menu_show_task("SOLVE", 0U, "NO COLOR");
        delay_ms(700);
        ui_menu_show_main();
        return;
    }

    g_face_on_stepper_2 = 'F';
    solve_and_execute(cube_str);
    if (emergency_stop_requested()) {
        ui_menu_show_main();
        return;
    }
    ui_menu_show_task("SOLVE", 100U, "DONE");
    delay_ms(700);
    ui_menu_show_main();
}

static bool scramble_table_read_move(const char **cursor, char *move)
{
    const char *p;

    if (cursor == NULL || *cursor == NULL || move == NULL) {
        return false;
    }

    p = *cursor;
    while (*p == ' ') {
        p++;
    }

    if (*p == '\0') {
        *cursor = p;
        return false;
    }

    if (*p != 'U' && *p != 'R' && *p != 'F' &&
        *p != 'D' && *p != 'L' && *p != 'B') {
        *cursor = p + 1;
        return false;
    }

    move[0] = *p++;
    if (*p == '\'' || *p == '2') {
        move[1] = *p++;
        move[2] = '\0';
    } else {
        move[1] = '\0';
        move[2] = '\0';
    }

    while (*p == ' ') {
        p++;
    }
    *cursor = p;
    return true;
}

static uint16_t count_solution_moves(const char *solution)
{
    const char *cursor = solution;
    char move[3];
    uint16_t count = 0U;

    while (scramble_table_read_move(&cursor, move)) {
        count++;
    }

    return count;
}

static uint16_t parse_solution_moves(const char *solution, char moves[][3], uint16_t max_moves)
{
    const char *cursor = solution;
    char move[3];
    uint16_t count = 0U;

    while (count < max_moves && scramble_table_read_move(&cursor, move)) {
        moves[count][0] = move[0];
        moves[count][1] = move[1];
        moves[count][2] = '\0';
        count++;
    }

    return count;
}

static void pattern_copy_move(const char *src, char *dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = '\0';
}

static void pattern_invert_move(const char *src, char *dst)
{
    dst[0] = src[0];
    if (src[1] == '2') {
        dst[1] = '2';
    } else if (src[1] == '\'') {
        dst[1] = '\0';
    } else {
        dst[1] = '\'';
    }
    dst[2] = '\0';
}

static void pattern_append_move(char **out, const char *move)
{
    *(*out)++ = move[0];
    if (move[1] != '\0') {
        *(*out)++ = move[1];
    }
}

static bool pattern_is_ud_pair(const char *first, const char *second)
{
    return ((first[0] == 'U' && second[0] == 'D') ||
            (first[0] == 'D' && second[0] == 'U'));
}

static void execute_pattern_moves(const char *title, char moves[][3],
                                  uint16_t total_moves, uint8_t reverse)
{
    uint16_t done_moves = 0U;
    int16_t index;

    if (total_moves == 0U) {
        return;
    }

    index = (reverse != 0U) ? (int16_t)(total_moves - 1U) : 0;
    while (done_moves < total_moves) {
        char current_move[3];
        char next_move[3];
        char exec_move[8];
        char *p = exec_move;
        uint16_t move_count = 1U;
        uint8_t percent;

        if (emergency_stop_requested()) {
            return;
        }

        if (reverse != 0U) {
            pattern_invert_move(moves[index], current_move);
        } else {
            pattern_copy_move(moves[index], current_move);
        }
        pattern_append_move(&p, current_move);

        if (done_moves + 1U < total_moves) {
            if (reverse != 0U) {
                pattern_invert_move(moves[index - 1], next_move);
            } else {
                pattern_copy_move(moves[index + 1], next_move);
            }

            if (pattern_is_ud_pair(current_move, next_move)) {
                *p++ = ' ';
                pattern_append_move(&p, next_move);
                index = (int16_t)(index + ((reverse != 0U) ? -1 : 1));
                move_count = 2U;
            }
        }

        *p = '\0';
        percent = (uint8_t)(((uint32_t)done_moves * 100UL) / (uint32_t)total_moves);
        ui_menu_show_pattern(title, percent);
        motion_ui_set_pattern(title, percent);
        g_face_on_stepper_2 = cube_app_execute_solution(g_face_on_stepper_2, exec_move);
        motion_ui_disable();
        if (emergency_stop_requested()) {
            return;
        }
        done_moves = (uint16_t)(done_moves + move_count);
        index = (int16_t)(index + ((reverse != 0U) ? -1 : 1));
    }

    ui_menu_show_pattern(title, 100U);
}

static void status_write_2digits(char **out, uint16_t value)
{
    if (value > 99U) {
        value = 99U;
    }
    *(*out)++ = (char)('0' + (value / 10U));
    *(*out)++ = (char)('0' + (value % 10U));
}

static void format_scramble_move_status(char *out, uint8_t table_index,
                                        const char *move,
                                        uint16_t done_step,
                                        uint16_t total_steps)
{
    char *p = out;

    *p++ = '#';
    status_write_2digits(&p, (uint16_t)(table_index + 1U));
    *p++ = ' ';
    *p++ = move[0];
    if (move[1] != '\0') {
        *p++ = move[1];
    }
    *p++ = ' ';
    status_write_2digits(&p, done_step);
    *p++ = '/';
    status_write_2digits(&p, total_steps);
    *p = '\0';
}

static void format_scramble_done_status(char *out, uint8_t table_index,
                                        uint16_t steps,
                                        uint16_t run_count)
{
    char *p = out;

    *p++ = '#';
    status_write_2digits(&p, (uint16_t)(table_index + 1U));
    *p++ = ' ';
    status_write_2digits(&p, steps);
    *p++ = ' ';
    *p++ = 'R';
    status_write_2digits(&p, (uint16_t)(run_count % 100U));
    *p = '\0';
}

static ui_scramble_done_action_t run_scramble_cycle(uint16_t steps)
{
    char move[3];
    char status[16];
    const char *cursor;
    uint16_t done = 0U;
    uint8_t table_index;
    uint8_t percent;

    if (steps == 0U) {
        return UI_SCRAMBLE_DONE_HOME;
    }

    prepare_cube_for_menu_action();
    if (emergency_stop_requested()) {
        return UI_SCRAMBLE_DONE_HOME;
    }

    table_index = g_scramble_table_index;
    cursor = g_scramble_table[table_index];
    g_last_scramble_table_index = table_index;
    g_last_scramble_steps = steps;
    g_scramble_run_count++;
    g_face_on_stepper_2 = 'F';

    while (done < steps) {
        if (emergency_stop_requested()) {
            return UI_SCRAMBLE_DONE_HOME;
        }
        if (!scramble_table_read_move(&cursor, move)) {
            table_index++;
            if (table_index >= SCRAMBLE_TABLE_COUNT) {
                table_index = 0U;
            }
            cursor = g_scramble_table[table_index];
            continue;
        }

        percent = (uint8_t)(((uint32_t)done * 100UL) / (uint32_t)steps);
        format_scramble_move_status(status,
                                    table_index,
                                    move,
                                    (uint16_t)(done + 1U),
                                    steps);
        ui_menu_show_scramble_step((uint16_t)(steps - done), percent, status);

        motion_ui_set_scramble((uint16_t)(steps - done), percent, status);
        g_face_on_stepper_2 = cube_app_execute_solution(g_face_on_stepper_2, move);
        motion_ui_disable();
        if (emergency_stop_requested()) {
            return UI_SCRAMBLE_DONE_HOME;
        }
        done++;
    }

    g_scramble_table_index++;
    if (g_scramble_table_index >= SCRAMBLE_TABLE_COUNT) {
        g_scramble_table_index = 0U;
    }

    format_scramble_done_status(status,
                                g_last_scramble_table_index,
                                g_last_scramble_steps,
                                g_scramble_run_count);
    ui_menu_show_scramble_step(0U, 100U, status);
    delay_ms(300);
    return ui_menu_wait_scramble_done_action();
}

static void run_scramble_menu_flow(uint16_t steps)
{
    ui_scramble_done_action_t action;

    do {
        action = run_scramble_cycle(steps);
        if (emergency_stop_requested()) {
            ui_menu_show_main();
            return;
        }
        if (action == UI_SCRAMBLE_DONE_SOLVE) {
            run_solve_cycle(0U);
            return;
        }
    } while (action == UI_SCRAMBLE_DONE_AGAIN);

    ui_menu_show_main();
}

static void run_pattern_cycle(ui_pattern_t pattern)
{
    const pattern_entry_t *entry;
    char moves[PATTERN_MAX_MOVES][3];
    uint16_t total_moves;
    ui_pattern_done_action_t done_action;

    if (pattern >= UI_PATTERN_COUNT) {
        ui_menu_show_main();
        return;
    }

    entry = &g_pattern_table[pattern];
    total_moves = count_solution_moves(entry->formula);
    if (total_moves == 0U) {
        ui_menu_show_main();
        return;
    }

    if (total_moves > PATTERN_MAX_MOVES) {
        total_moves = PATTERN_MAX_MOVES;
    }
    total_moves = parse_solution_moves(entry->formula, moves, total_moves);
    if (total_moves == 0U) {
        ui_menu_show_main();
        return;
    }

    prepare_cube_for_menu_action();
    if (emergency_stop_requested()) {
        ui_menu_show_main();
        return;
    }

    g_face_on_stepper_2 = 'F';
    execute_pattern_moves(entry->name, moves, total_moves, 0U);
    if (emergency_stop_requested()) {
        ui_menu_show_main();
        return;
    }
    delay_ms(300);

    done_action = ui_menu_wait_pattern_done_action();
    if (emergency_stop_requested()) {
        ui_menu_show_main();
        return;
    }
    if (done_action == UI_PATTERN_DONE_RETURN) {
        execute_pattern_moves("Return", moves, total_moves, 1U);
        if (emergency_stop_requested()) {
            ui_menu_show_main();
            return;
        }
        delay_ms(500);
    }

    ui_menu_show_main();
}

int main(void)
{
    system_clock_config();
    delay_init();
    io_init();
    bsp_hmi_init();
    ui_menu_init();

    g_color_ready = (color_sensor_init() == COLOR_RET_OK);

    cube_app_init();
    stepper_set_idle_callback(motion_ui_service);
    solve_set_abort_callback(solve_abort_poll);
    ui_menu_show_main();

    while (1) {
        ui_command_t ui_command = ui_menu_update(0U);

        if (emergency_stop_poll() || ui_command == UI_COMMAND_EMERGENCY_STOP) {
            emergency_stop_return_to_main();
            emergency_stop_clear_for_next_action();
        } else if (ui_command == UI_COMMAND_SOLVE_START) {
            emergency_stop_clear_for_next_action();
            run_solve_cycle(1U);
            if (emergency_stop_requested()) {
                emergency_stop_return_to_main();
                emergency_stop_clear_for_next_action();
            }
        } else if (ui_command == UI_COMMAND_SCRAMBLE_START) {
            emergency_stop_clear_for_next_action();
            run_scramble_menu_flow(ui_menu_get_scramble_steps());
            if (emergency_stop_requested()) {
                emergency_stop_return_to_main();
                emergency_stop_clear_for_next_action();
            }
        } else if (ui_command == UI_COMMAND_PATTERN_START) {
            emergency_stop_clear_for_next_action();
            run_pattern_cycle(ui_menu_get_pattern());
            if (emergency_stop_requested()) {
                emergency_stop_return_to_main();
                emergency_stop_clear_for_next_action();
            }
        }

        if (button_pressed()) {
            delay_ms(10);
            while (button_pressed()) {
                delay_ms(10);
            }
            emergency_stop_clear_for_next_action();
            run_solve_cycle(1U);
            if (emergency_stop_requested()) {
                emergency_stop_return_to_main();
                emergency_stop_clear_for_next_action();
            }
        }

        delay_ms(10);
    }
}
