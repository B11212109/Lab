#ifndef UI_MENU_H
#define UI_MENU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_STATE_MAIN = 0,
    UI_STATE_SOLVE,
    UI_STATE_SCRAMBLE,
    UI_STATE_PATTERN,
    UI_STATE_STANDBY
} ui_state_t;

typedef enum {
    UI_COMMAND_NONE = 0,
    UI_COMMAND_SOLVE_START,
    UI_COMMAND_SCRAMBLE_START,
    UI_COMMAND_PATTERN_START,
    UI_COMMAND_STANDBY_START,
    UI_COMMAND_MOTOR_HOMING_TEST,
    UI_COMMAND_EMERGENCY_STOP
} ui_command_t;

typedef enum {
    UI_PATTERN_CHECKER = 0,
    UI_PATTERN_BOX,
    UI_PATTERN_CROSS,
    UI_PATTERN_SLASH,
    UI_PATTERN_CUBE3,
    UI_PATTERN_FISH,
    UI_PATTERN_SIX_THREE_LINES,
    UI_PATTERN_BOW_ARROW,
    UI_PATTERN_SIX_CU,
    UI_PATTERN_SNAKE,
    UI_PATTERN_COUNT
} ui_pattern_t;

typedef enum {
    UI_SCRAMBLE_DONE_SOLVE = 0,
    UI_SCRAMBLE_DONE_AGAIN,
    UI_SCRAMBLE_DONE_HOME
} ui_scramble_done_action_t;

typedef enum {
    UI_PATTERN_DONE_RETURN = 0,
    UI_PATTERN_DONE_CONTINUE
} ui_pattern_done_action_t;

void ui_menu_init(void);
ui_command_t ui_menu_update(uint8_t progress_percent);
void ui_menu_show_task(const char *title, uint8_t percent, const char *status);
void ui_menu_show_task_frame(const char *title, uint8_t percent, const char *status,
                             uint16_t frame_index);
void ui_menu_show_error(const char *status);
void ui_menu_show_scramble_step(uint16_t remaining_steps, uint8_t percent, const char *status);
void ui_menu_show_scramble_step_frame(uint16_t remaining_steps, uint8_t percent,
                                      const char *status, uint16_t frame_index);
ui_scramble_done_action_t ui_menu_wait_scramble_done_action(void);
void ui_menu_show_homing_progress(void);
void ui_menu_wait_place_cube(void);
void ui_menu_show_standby_boot(void);
void ui_menu_show_main(void);
void ui_menu_show_motor_calib(void);
ui_state_t ui_menu_get_state(void);
uint16_t ui_menu_get_scramble_steps(void);
void ui_menu_emit_command(ui_command_t command);
void ui_menu_request_emergency_stop(void);
uint8_t ui_menu_is_emergency_stop_requested(void);
void ui_menu_clear_emergency_stop(void);
void ui_menu_select_pattern(ui_pattern_t pattern);
ui_pattern_t ui_menu_get_pattern(void);
void ui_menu_show_pattern(const char *pattern_name, uint8_t percent);
void ui_menu_show_pattern_frame(const char *pattern_name, uint8_t percent,
                                uint16_t frame_index);
ui_pattern_done_action_t ui_menu_wait_pattern_done_action(void);

#ifdef __cplusplus
}
#endif

#endif
