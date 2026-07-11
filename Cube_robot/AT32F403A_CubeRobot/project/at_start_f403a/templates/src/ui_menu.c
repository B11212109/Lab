#include "ui_menu.h"

#include "OLED_UI.h"
#include "OLED_UI_Launcher.h"
#include "OLED_UI_MenuData.h"

extern MenuPage *CurrentMenuPage;

static ui_state_t g_ui_state = UI_STATE_MAIN;
static ui_command_t g_pending_command = UI_COMMAND_NONE;
static uint16_t g_active_scramble_steps = 21U;
static ui_pattern_t g_pending_pattern = UI_PATTERN_CHECKER;
static uint8_t g_emergency_stop_requested = 0U;

void ui_menu_emit_command(ui_command_t command)
{
    if (command == UI_COMMAND_NONE) {
        return;
    }

    g_pending_command = command;
    if (command == UI_COMMAND_SOLVE_START) {
        g_ui_state = UI_STATE_SOLVE;
    } else if (command == UI_COMMAND_SCRAMBLE_START) {
        g_ui_state = UI_STATE_SCRAMBLE;
        g_active_scramble_steps = OLED_UI_MenuData_GetScrambleSteps();
    } else if (command == UI_COMMAND_PATTERN_START) {
        g_ui_state = UI_STATE_PATTERN;
    } else if (command == UI_COMMAND_STANDBY_START) {
        g_ui_state = UI_STATE_STANDBY;
    } else if (command == UI_COMMAND_MOTOR_HOMING_TEST) {
        g_ui_state = UI_STATE_MAIN;
    } else if (command == UI_COMMAND_EMERGENCY_STOP) {
        g_emergency_stop_requested = 1U;
        g_ui_state = UI_STATE_MAIN;
    }
}

void ui_menu_request_emergency_stop(void)
{
    g_emergency_stop_requested = 1U;
    g_pending_command = UI_COMMAND_EMERGENCY_STOP;
    g_ui_state = UI_STATE_MAIN;
}

uint8_t ui_menu_is_emergency_stop_requested(void)
{
    return g_emergency_stop_requested;
}

void ui_menu_clear_emergency_stop(void)
{
    g_emergency_stop_requested = 0U;
    if (g_pending_command == UI_COMMAND_EMERGENCY_STOP) {
        g_pending_command = UI_COMMAND_NONE;
    }
}

void ui_menu_select_pattern(ui_pattern_t pattern)
{
    if (pattern >= UI_PATTERN_COUNT) {
        return;
    }

    g_pending_pattern = pattern;
    ui_menu_emit_command(UI_COMMAND_PATTERN_START);
}

void ui_menu_init(void)
{
    g_ui_state = UI_STATE_MAIN;
    g_pending_command = UI_COMMAND_NONE;
    g_active_scramble_steps = OLED_UI_MenuData_GetScrambleSteps();
    g_pending_pattern = UI_PATTERN_CHECKER;
    g_emergency_stop_requested = 0U;
    OLED_UI_init();
}

ui_command_t ui_menu_update(uint8_t progress_percent)
{
    ui_command_t command;

    (void)progress_percent;

    OLED_UI_MainLoop();
    OLED_UI_InterruptHandler();
    OLED_UI_Tick(10U);

    command = g_pending_command;
    g_pending_command = UI_COMMAND_NONE;
    return command;
}

void ui_menu_show_task(const char *title, uint8_t percent, const char *status)
{
    OLED_UI_MenuData_ShowTask(title, percent, status);
}

void ui_menu_show_task_frame(const char *title, uint8_t percent, const char *status,
                             uint16_t frame_index)
{
    OLED_UI_MenuData_ShowTaskFrame(title, percent, status, frame_index);
}

void ui_menu_show_error(const char *status)
{
    OLED_UI_MenuData_ShowError(status);
}

void ui_menu_show_scramble_step(uint16_t remaining_steps, uint8_t percent,
                                const char *status)
{
    OLED_UI_MenuData_ShowScrambleStep(remaining_steps, percent, status);
}

void ui_menu_show_scramble_step_frame(uint16_t remaining_steps, uint8_t percent,
                                      const char *status, uint16_t frame_index)
{
    OLED_UI_MenuData_ShowScrambleStepFrame(remaining_steps, percent, status, frame_index);
}

void ui_menu_show_pattern(const char *pattern_name, uint8_t percent)
{
    OLED_UI_MenuData_ShowPatternProgress(pattern_name, percent);
}

void ui_menu_show_pattern_frame(const char *pattern_name, uint8_t percent,
                                uint16_t frame_index)
{
    OLED_UI_MenuData_ShowPatternFrame(pattern_name, percent, frame_index);
}

ui_pattern_done_action_t ui_menu_wait_pattern_done_action(void)
{
    uint8_t action = OLED_UI_MenuData_WaitPatternDoneAction();

    if (action == 0U) {
        return UI_PATTERN_DONE_RETURN;
    }
    return UI_PATTERN_DONE_CONTINUE;
}

ui_scramble_done_action_t ui_menu_wait_scramble_done_action(void)
{
    uint8_t action = OLED_UI_MenuData_WaitScrambleDoneAction();

    if (action == 0U) {
        return UI_SCRAMBLE_DONE_SOLVE;
    }
    if (action == 1U) {
        return UI_SCRAMBLE_DONE_AGAIN;
    }
    return UI_SCRAMBLE_DONE_HOME;
}

void ui_menu_show_homing_progress(void)
{
    OLED_UI_MenuData_ShowHomingOnly();
}

void ui_menu_wait_place_cube(void)
{
    OLED_UI_MenuData_WaitPlaceCube();
}

void ui_menu_show_standby_boot(void)
{
    OLED_UI_MenuData_ShowStandbyBoot();
}

void ui_menu_show_main(void)
{
    CurrentMenuPage = &MainMenuPage;
    CurrentMenuPageInit();
    ResetEnterFlag();
    ResetFadeOutFlag();
    g_ui_state = UI_STATE_MAIN;
    g_pending_command = UI_COMMAND_NONE;

    OLED_UI_MainLoop();
    OLED_Update();
}

void ui_menu_show_motor_calib(void)
{
    OLED_UI_MenuData_OpenMotorCalibrationWindow();
}

ui_state_t ui_menu_get_state(void)
{
    return g_ui_state;
}

uint16_t ui_menu_get_scramble_steps(void)
{
    return g_active_scramble_steps;
}

ui_pattern_t ui_menu_get_pattern(void)
{
    return g_pending_pattern;
}
