#include "OLED_UI_MenuData.h"
#include "bsp_hmi.h"
#include "OLED_assets/cube3D_loder.h"
#include "OLED_assets/pattern_loading.h"
#include "OLED_assets/rubik_loader_64x64.h"

static int16_t g_scramble_steps = 20;
static int16_t g_turn_speed_percent = 100;
static uint16_t g_fps_show_ticks = 0U;

#define TURN_SPEED_DEFAULT_ENCODER_PERCENT      50U
#define OLED_BRIGHTNESS_DEFAULT_PERCENT         50U

extern int16_t OLED_UI_Brightness;
extern OLED_UI_Counter OLED_FPS;

static uint8_t GetAsciiTextWidth(const char *text, uint8_t font_size)
{
    uint8_t width = 0U;

    while (*text != '\0') {
        width = (uint8_t)(width + font_size);
        text++;
    }

    return width;
}

static int16_t GetCenteredTextX(const char *text, uint8_t font_size)
{
    uint8_t width = GetAsciiTextWidth(text, font_size);

    if (width >= OLED_WIDTH) {
        return 0;
    }

    return (int16_t)((OLED_WIDTH - width) / 2U);
}

static void DrawCenteredString(int16_t y, const char *text, uint8_t font_size)
{
    OLED_ShowString(GetCenteredTextX(text, font_size), y, (char *)text, font_size);
}

static int16_t ReadTurnSpeedPercent(void)
{
    return (int16_t)(50U + hmi_get_speed_percent());
}

static int16_t ReadScrambleStepTarget(void)
{
    return (int16_t)(15U + ((uint16_t)hmi_get_speed_percent() * 10U + 50U) / 100U);
}

static void DrawOKButton(void)
{
    OLED_DrawRectangle(54, 41, 20, 11, OLED_UNFILLED);
    OLED_ShowString(58, 43, "OK", OLED_6X8_HALF);
}

static const uint8_t *GetTinyGlyph(char ch)
{
    static const uint8_t glyph_space[5] = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    static const uint8_t glyph_c[5] = {0x07U, 0x04U, 0x04U, 0x04U, 0x07U};
    static const uint8_t glyph_e[5] = {0x07U, 0x04U, 0x07U, 0x04U, 0x07U};
    static const uint8_t glyph_i[5] = {0x07U, 0x02U, 0x02U, 0x02U, 0x07U};
    static const uint8_t glyph_n[5] = {0x05U, 0x07U, 0x07U, 0x07U, 0x05U};
    static const uint8_t glyph_o[5] = {0x07U, 0x05U, 0x05U, 0x05U, 0x07U};
    static const uint8_t glyph_p[5] = {0x07U, 0x05U, 0x07U, 0x04U, 0x04U};
    static const uint8_t glyph_r[5] = {0x06U, 0x05U, 0x06U, 0x05U, 0x05U};
    static const uint8_t glyph_s[5] = {0x07U, 0x04U, 0x07U, 0x01U, 0x07U};
    static const uint8_t glyph_t[5] = {0x07U, 0x02U, 0x02U, 0x02U, 0x02U};
    static const uint8_t glyph_u[5] = {0x05U, 0x05U, 0x05U, 0x05U, 0x07U};

    switch (ch) {
    case 'C':
        return glyph_c;
    case 'E':
        return glyph_e;
    case 'I':
        return glyph_i;
    case 'N':
        return glyph_n;
    case 'O':
        return glyph_o;
    case 'P':
        return glyph_p;
    case 'R':
        return glyph_r;
    case 'S':
        return glyph_s;
    case 'T':
        return glyph_t;
    case 'U':
        return glyph_u;
    default:
        return glyph_space;
    }
}

static void DrawTinyString(int16_t x, int16_t y, const char *text)
{
    const uint8_t *glyph;
    uint8_t row;
    uint8_t col;

    while (*text != '\0') {
        glyph = GetTinyGlyph(*text);
        for (row = 0U; row < 5U; row++) {
            for (col = 0U; col < 3U; col++) {
                if ((glyph[row] & (uint8_t)(0x04U >> col)) != 0U) {
                    OLED_DrawPoint(x + col, y + row);
                }
            }
        }
        x += 4;
        text++;
    }
}

static void DrawCenteredPercent(int16_t y, uint16_t percent)
{
    int16_t text_w;

    if (percent > 999U) {
        percent = 999U;
    }

    if (percent >= 100U) {
        text_w = 24;
    } else if (percent >= 10U) {
        text_w = 18;
    } else {
        text_w = 12;
    }

    OLED_Printf((128 - text_w) / 2, y, OLED_6X8_HALF, "%d%%", percent);
}

static uint8_t GetCube3DLoadingPixel(uint8_t frame_index, uint8_t row, uint8_t col)
{
    uint8_t byte_value;
    uint8_t mask;

    if (frame_index >= TUMBLING_RUBIK_FRAME_COUNT ||
        row >= TUMBLING_RUBIK_HEIGHT || col >= TUMBLING_RUBIK_WIDTH) {
        return 0U;
    }

    byte_value = tumbling_rubik_64x64_page[frame_index][((uint16_t)(row / 8U) * TUMBLING_RUBIK_WIDTH) + col];
    mask = (uint8_t)(1U << (row & 0x07U));
    return (byte_value & mask) != 0U;
}

static void DrawCube3DLoadingFrameArea(int16_t x, int16_t y, uint8_t size, uint8_t frame_index)
{
    uint8_t row;
    uint8_t col;
    uint8_t src_row;
    uint8_t src_col;

    if (size == 0U) {
        return;
    }
    if (size > TUMBLING_RUBIK_WIDTH) {
        size = TUMBLING_RUBIK_WIDTH;
    }

    for (row = 0U; row < size; row++) {
        src_row = (uint8_t)(((uint16_t)row * TUMBLING_RUBIK_HEIGHT) / size);
        for (col = 0U; col < size; col++) {
            src_col = (uint8_t)(((uint16_t)col * TUMBLING_RUBIK_WIDTH) / size);
            if (GetCube3DLoadingPixel(frame_index, src_row, src_col) != 0U) {
                OLED_DrawPoint(x + col, y + row);
            }
        }
    }
}

static uint8_t GetRubikLoaderPixel(const uint8_t *frame, uint8_t row, uint8_t col)
{
    uint8_t byte_value;
    uint8_t mask;

    if (row >= RUBIK_LOADER_H || col >= RUBIK_LOADER_W) {
        return 0U;
    }

    byte_value = frame[((uint16_t)row * RUBIK_LOADER_W + col) / 8U];
    mask = (uint8_t)(0x80U >> (col & 0x07U));
    return (byte_value & mask) != 0U;
}

static uint8_t IsRubikLoaderEdgePixel(const uint8_t *frame, uint8_t row, uint8_t col)
{
    if (GetRubikLoaderPixel(frame, row, col) == 0U) {
        return 0U;
    }

    if (row == 0U || row >= (RUBIK_LOADER_H - 1U) ||
        col == 0U || col >= (RUBIK_LOADER_W - 1U)) {
        return 1U;
    }

    if (GetRubikLoaderPixel(frame, (uint8_t)(row - 1U), col) == 0U ||
        GetRubikLoaderPixel(frame, (uint8_t)(row + 1U), col) == 0U ||
        GetRubikLoaderPixel(frame, row, (uint8_t)(col - 1U)) == 0U ||
        GetRubikLoaderPixel(frame, row, (uint8_t)(col + 1U)) == 0U) {
        return 1U;
    }

    return 0U;
}

static void DrawRubikLoaderFrameArea(int16_t x, int16_t y, uint8_t size,
                                     uint8_t progress, const uint8_t *frame)
{
    uint8_t row;
    uint8_t col;
    uint8_t src_row;
    uint8_t src_col;
    uint8_t fill_start;
    uint8_t draw_pixel;

    if (size == 0U) {
        return;
    }
    if (size > RUBIK_LOADER_W) {
        size = RUBIK_LOADER_W;
    }
    if (progress > 100U) {
        progress = 100U;
    }

    fill_start = (uint8_t)(size - ((uint16_t)size * progress) / 100U);

    for (row = 0U; row < size; row++) {
        src_row = (uint8_t)(((uint16_t)row * RUBIK_LOADER_H) / size);
        for (col = 0U; col < size; col++) {
            src_col = (uint8_t)(((uint16_t)col * RUBIK_LOADER_W) / size);
            draw_pixel = IsRubikLoaderEdgePixel(frame, src_row, src_col);
            if (row >= fill_start && GetRubikLoaderPixel(frame, src_row, src_col) != 0U) {
                draw_pixel = 1U;
            }
            if (draw_pixel != 0U) {
                OLED_DrawPoint(x + col, y + row);
            }
        }
    }
}

static MenuWindow ScrambleStartWindow = {
    .General_Width = 100,
    .General_Height = 30,
    .General_ContinueTime = 2.0f,
    .General_WindowType = WINDOW_ROUNDRECTANGLE,
    .Text_String = "Scramble Ready",
    .Text_FontSize = OLED_UI_FONT_12,
    .Text_FontSideDistance = 4,
    .Text_FontTopDistance = 3,
    .Prob_Data_Int = &g_scramble_steps,
    .Prob_DataStep = 1,
    .Prob_MinData = 1,
    .Prob_MaxData = 99,
    .Prob_BottomDistance = 3,
    .Prob_SideDistance = 4,
    .Prob_LineHeight = 8,
};

static void ShowHomingProgress(void);
static void ShowSolveProgressWindow(uint8_t run_homing);
static uint8_t ShowScrambleStartWindow(void);
static void ShowPatternWindow(const char *pattern_name);

static void DrawSolveProgress(uint8_t percent, const char *status)
{
    uint8_t frame_index;

    (void)status;
    frame_index = (uint8_t)(((uint16_t)percent * (TUMBLING_RUBIK_FRAME_COUNT - 1U)) / 100U);
    OLED_Clear();
    OLED_ShowString(29, 5, "Solving Cube", OLED_6X8_HALF);
    DrawCube3DLoadingFrameArea(42, 12, 44U, frame_index);
    DrawCenteredPercent(56, percent);
    OLED_Update();
}

static void ShowSolveProgressWindow(uint8_t run_homing)
{
    static const char *status_table[] = {
        "Scanning colors",
        "Building cube map",
        "Planning moves",
        "Turning U face",
        "Turning R face",
        "Flipping cube",
        "Checking result",
        "Solved"
    };
    uint16_t percent;
    uint8_t speed_percent;
    uint8_t status_index;
    uint32_t frame_delay;

    if (run_homing != 0U) {
        ShowHomingProgress();
    }

    for (percent = 0U; percent <= 100U; percent++) {
        speed_percent = (uint8_t)ReadTurnSpeedPercent();
        g_turn_speed_percent = speed_percent;
        status_index = (uint8_t)(percent * 7U / 100U);
        DrawSolveProgress((uint8_t)percent, status_table[status_index]);

        frame_delay = 120U - ((uint32_t)speed_percent / 2U);
        if (frame_delay < 25U) {
            frame_delay = 25U;
        }
        Delay_ms(frame_delay);
    }

    Delay_ms(500U);
}

static void ShowSolveWindow(void)
{
    ShowSolveProgressWindow(1U);
}

static void ShowHomingProgress(void)
{
    static const char *homing_text[] = {
        "Homing",
        "Homing.",
        "Homing..",
        "Homing..."
    };
    uint8_t step;
    uint8_t blink;
    uint8_t i;
    uint8_t prompt_x;
    uint8_t prompt_w;

    for (step = 0U; step < 12U; step++) {
        OLED_Clear();
        OLED_ShowString(32, 24, (char *)homing_text[step & 0x03U], OLED_8X16_HALF);
        OLED_Update();
        Delay_ms(250U);
    }

    for (blink = 0U;; blink ^= 1U) {
        OLED_Clear();
        OLED_ShowString(22, 16, "PLEASE PLACE", OLED_7X12_HALF);
        OLED_ShowString(36, 30, "THE CUBE", OLED_7X12_HALF);

        prompt_w = 67U;
        prompt_x = (uint8_t)((128U - prompt_w) / 2U);
        if (blink != 0U) {
            DrawTinyString(prompt_x, 54, "PRESS TO COUTUINE");
        }
        OLED_Update();

        for (i = 0U; i < 5U; i++) {
            if (hmi_get_action() == ACTION_CLICK) {
                return;
            }
            Delay_ms(100U);
        }
    }
}

static void ShowDashboardWindow(void)
{
    static const uint8_t popup_w[] = {24U, 48U, 70U, 88U};
    static const uint8_t popup_h[] = {10U, 16U, 25U, 32U};
    uint8_t i;
    uint8_t width;
    uint8_t height;
    uint8_t x;
    uint8_t y;
    uint8_t bar_width;

    ResetEnterFlag();
    for (i = 0U; i < 4U; i++) {
        width = popup_w[i];
        height = popup_h[i];
        x = (uint8_t)((128U - width) / 2U);
        y = (uint8_t)((64U - height) / 2U);
        OLED_ClearArea(x, y, width, height);
        OLED_DrawRoundedRectangle(x, y, width, height, 2, OLED_UNFILLED);
        OLED_Update();
        Delay_ms(25U);
    }

    for (;;) {
        if (hmi_consume_reset_button() != 0U) {
            hmi_set_speed_percent(TURN_SPEED_DEFAULT_ENCODER_PERCENT);
        }
        g_turn_speed_percent = ReadTurnSpeedPercent();

        OLED_ClearArea(20, 11, 88, 44);
        OLED_DrawRoundedRectangle(20, 11, 88, 44, 2, OLED_UNFILLED);
        OLED_ShowString(31, 16, "Motor Speed", OLED_6X8_HALF);
        OLED_DrawRoundedRectangle(27, 28, 74, 12, 2, OLED_UNFILLED);
        bar_width = (uint8_t)(((uint16_t)(g_turn_speed_percent - 50) * 68U + 50U) / 100U);
        if (bar_width != 0U) {
            OLED_DrawRectangle(30, 31, bar_width, 6, OLED_FILLED);
        }
        OLED_ClearArea(51, 30, 26, 8);
        OLED_Printf(52, 30, OLED_6X8_HALF, "%3d%%", g_turn_speed_percent);
        DrawOKButton();
        OLED_Update();

        if (hmi_get_action() == ACTION_CLICK) {
            return;
        }
        Delay_ms(50U);
    }
}

static void ShowBrightnessWindow(void)
{
    uint8_t bar_width;

    ResetEnterFlag();
    for (;;) {
        if (hmi_consume_reset_button() != 0U) {
            hmi_set_speed_percent(OLED_BRIGHTNESS_DEFAULT_PERCENT);
        }
        OLED_UI_Brightness = (int16_t)(((uint16_t)hmi_get_speed_percent() * 255U + 50U) / 100U);

        OLED_ClearArea(20, 11, 88, 44);
        OLED_DrawRoundedRectangle(20, 11, 88, 44, 2, OLED_UNFILLED);
        OLED_ShowString(34, 16, "Brightness", OLED_6X8_HALF);
        OLED_DrawRoundedRectangle(27, 28, 74, 12, 2, OLED_UNFILLED);
        bar_width = (uint8_t)(((uint16_t)OLED_UI_Brightness * 68U + 127U) / 255U);
        if (bar_width != 0U) {
            OLED_DrawRectangle(30, 31, bar_width, 6, OLED_FILLED);
        }
        OLED_ClearArea(51, 30, 26, 8);
        OLED_Printf(52, 30, OLED_6X8_HALF, "%3d%%", (uint16_t)hmi_get_speed_percent());
        DrawOKButton();
        OLED_Update();

        if (hmi_get_action() == ACTION_CLICK) {
            return;
        }
        Delay_ms(50U);
    }
}

static void ShowFPSForFiveSeconds(void)
{
    g_fps_show_ticks = 250U;
}

static void DrawTimedFPS(void)
{
    if (g_fps_show_ticks == 0U) {
        return;
    }

    OLED_ClearArea(86, 0, 42, 8);
    OLED_Printf(86, 0, OLED_6X8_HALF, "FPS:%2d", OLED_FPS.value);
    g_fps_show_ticks--;
}

static void DrawScrambleStepSelector(int16_t display_steps_tenths)
{
    int16_t previous_steps;
    int16_t current_steps;
    int16_t next_steps;
    int16_t y_offset;

    current_steps = (display_steps_tenths + 5) / 10;
    if (current_steps < 15) {
        current_steps = 15;
    }
    if (current_steps > 25) {
        current_steps = 25;
    }

    previous_steps = current_steps - 1;
    next_steps = current_steps + 1;
    y_offset = (int16_t)((display_steps_tenths - current_steps * 10) / 2);

    OLED_Clear();
    DrawCenteredString(3, "Scramble Steps", OLED_6X8_HALF);
    if (previous_steps >= 15) {
        OLED_Printf(58, 18 - y_offset, OLED_6X8_HALF, "%02d", previous_steps);
    }
    OLED_Printf(54, 29 - y_offset, OLED_10X20_HALF, "%02d", current_steps);
    if (next_steps <= 25) {
        OLED_Printf(58, 52 - y_offset, OLED_6X8_HALF, "%02d", next_steps);
    }
    ReverseCoordinate(47, 28, 34, 22, REVERSE_ROUNDRECTANGLE);
    OLED_Update();
}

static uint8_t SmoothU8(uint8_t current, uint8_t target)
{
    uint8_t delta;

    if (current == target) {
        return current;
    }

    if (current < target) {
        delta = (uint8_t)(target - current);
        current = (uint8_t)(current + ((delta + 3U) / 4U));
        if (current > target) {
            current = target;
        }
    } else {
        delta = (uint8_t)(current - target);
        current = (uint8_t)(current - ((delta + 3U) / 4U));
        if (current < target) {
            current = target;
        }
    }

    return current;
}

static void DrawScrambleCompleteActions(uint8_t cursor_x, uint8_t cursor_y, uint8_t cursor_w)
{
    static const char *button_text[] = {"Solve", "Again", "Continue"};
    static const uint8_t button_y[] = {9U, 26U, 43U};
    static const uint8_t button_x = 28U;
    static const uint8_t button_w = 72U;
    static const uint8_t button_h = 13U;
    uint8_t i;
    uint8_t text_w;

    OLED_Clear();
    for (i = 0U; i < 3U; i++) {
        OLED_DrawRoundedRectangle(button_x, button_y[i], button_w, button_h, 2, OLED_UNFILLED);
        text_w = GetAsciiTextWidth(button_text[i], OLED_6X8_HALF);
        OLED_ShowString(button_x + (button_w - text_w) / 2U,
                        button_y[i] + 3U,
                        (char *)button_text[i],
                        OLED_6X8_HALF);
    }
    ReverseCoordinate(cursor_x, cursor_y, cursor_w, button_h, REVERSE_ROUNDRECTANGLE);
    OLED_Update();
}

static uint8_t WaitScrambleCompleteAction(void)
{
    static const uint8_t button_y[] = {9U, 26U, 43U};
    static const uint8_t button_x = 28U;
    static const uint8_t button_w = 72U;
    hmi_action_t action;
    uint8_t selected = 0U;
    uint8_t cursor_x = button_x;
    uint8_t cursor_y = button_y[0];
    uint8_t cursor_w = button_w;

    ResetEnterFlag();
    for (;;) {
        cursor_x = SmoothU8(cursor_x, button_x);
        cursor_y = SmoothU8(cursor_y, button_y[selected]);
        cursor_w = SmoothU8(cursor_w, button_w);
        DrawScrambleCompleteActions(cursor_x, cursor_y, cursor_w);
        action = hmi_get_action();
        if (action == ACTION_UP || action == ACTION_LEFT) {
            if (selected > 0U) {
                selected--;
            } else {
                selected = 2U;
            }
        } else if (action == ACTION_DOWN || action == ACTION_RIGHT) {
            selected++;
            if (selected >= 3U) {
                selected = 0U;
            }
        } else if (action == ACTION_CLICK) {
            return selected;
        }
        Delay_ms(20U);
    }
}

static void DrawPatternCompleteActions(uint8_t cursor_x, uint8_t cursor_y, uint8_t cursor_w)
{
    static const char *button_text[] = {"Return", "Continue"};
    static const uint8_t button_y[] = {18U, 35U};
    static const uint8_t button_x = 28U;
    static const uint8_t button_w = 72U;
    static const uint8_t button_h = 13U;
    uint8_t i;
    uint8_t text_w;

    OLED_Clear();
    for (i = 0U; i < 2U; i++) {
        OLED_DrawRoundedRectangle(button_x, button_y[i], button_w, button_h, 2, OLED_UNFILLED);
        text_w = GetAsciiTextWidth(button_text[i], OLED_6X8_HALF);
        OLED_ShowString(button_x + (button_w - text_w) / 2U,
                        button_y[i] + 3U,
                        (char *)button_text[i],
                        OLED_6X8_HALF);
    }
    ReverseCoordinate(cursor_x, cursor_y, cursor_w, button_h, REVERSE_ROUNDRECTANGLE);
    OLED_Update();
}

static uint8_t WaitPatternCompleteAction(void)
{
    static const uint8_t button_y[] = {18U, 35U};
    static const uint8_t button_x = 28U;
    static const uint8_t button_w = 72U;
    hmi_action_t action;
    uint8_t selected = 0U;
    uint8_t cursor_x = button_x;
    uint8_t cursor_y = button_y[0];
    uint8_t cursor_w = button_w;

    ResetEnterFlag();
    for (;;) {
        cursor_x = SmoothU8(cursor_x, button_x);
        cursor_y = SmoothU8(cursor_y, button_y[selected]);
        cursor_w = SmoothU8(cursor_w, button_w);
        DrawPatternCompleteActions(cursor_x, cursor_y, cursor_w);
        action = hmi_get_action();
        if (action == ACTION_UP || action == ACTION_LEFT) {
            selected ^= 1U;
        } else if (action == ACTION_DOWN || action == ACTION_RIGHT) {
            selected ^= 1U;
        } else if (action == ACTION_CLICK) {
            return selected;
        }
        Delay_ms(20U);
    }
}

static void ShowScrambleStepSelector(void)
{
    hmi_action_t action;
    uint8_t scramble_action;
    int16_t target_steps_tenths;
    int16_t display_steps_tenths;

    ResetEnterFlag();
    display_steps_tenths = (int16_t)(g_scramble_steps * 10);
    for (;;) {
        g_scramble_steps = ReadScrambleStepTarget();
        target_steps_tenths = (int16_t)(g_scramble_steps * 10);

        if (display_steps_tenths < target_steps_tenths) {
            display_steps_tenths += 2;
            if (display_steps_tenths > target_steps_tenths) {
                display_steps_tenths = target_steps_tenths;
            }
        } else if (display_steps_tenths > target_steps_tenths) {
            display_steps_tenths -= 2;
            if (display_steps_tenths < target_steps_tenths) {
                display_steps_tenths = target_steps_tenths;
            }
        }

        DrawScrambleStepSelector(display_steps_tenths);
        action = hmi_get_action();

        if (action == ACTION_CLICK) {
            g_scramble_steps = (display_steps_tenths + 5) / 10;
            for (;;) {
                scramble_action = ShowScrambleStartWindow();
                if (scramble_action == 0U) {
                    ShowSolveProgressWindow(0U);
                    return;
                }
                if (scramble_action != 1U) {
                    return;
                }
            }
        }

        Delay_ms(20U);
    }
}

static uint8_t ShowScrambleStartWindow(void)
{
    uint8_t frame_index;
    uint8_t loop_count;
    uint16_t progress;
    uint16_t frame_number;
    const uint16_t total_frames = (uint16_t)(2U * RUBIK_LOADER_FRAME_COUNT);

    (void)ScrambleStartWindow;
    ShowHomingProgress();

    for (loop_count = 0U; loop_count < 2U; loop_count++) {
        for (frame_index = 0U; frame_index < RUBIK_LOADER_FRAME_COUNT; frame_index++) {
            frame_number = (uint16_t)(loop_count * RUBIK_LOADER_FRAME_COUNT + frame_index + 1U);
            progress = (uint16_t)((frame_number * 100U) / total_frames);

            OLED_Clear();
            DrawCenteredString(2, "Scramble", OLED_6X8_HALF);
            DrawRubikLoaderFrameArea(42, 10, 44U, (uint8_t)progress, rubik_loader_frames[frame_index]);
            DrawCenteredPercent(56, progress);
            OLED_Update();
            Delay_ms(35U);
        }
    }
    Delay_ms(350U);
    return WaitScrambleCompleteAction();
}

static uint8_t GetPatternLoadingPixel(uint8_t frame_index, uint8_t row, uint8_t col)
{
    uint8_t byte_value;
    uint8_t mask;

    if (frame_index >= WAVE_GRID_FRAME_COUNT ||
        row >= WAVE_GRID_HEIGHT || col >= WAVE_GRID_WIDTH) {
        return 0U;
    }

    byte_value = wave_grid_64x64_page[frame_index][((uint16_t)(row / 8U) * WAVE_GRID_WIDTH) + col];
    mask = (uint8_t)(1U << (row & 0x07U));
    return (byte_value & mask) != 0U;
}

static void DrawPatternLoadingFrameArea(int16_t x, int16_t y, uint8_t size, uint8_t frame_index)
{
    uint8_t row;
    uint8_t col;
    uint8_t src_row;
    uint8_t src_col;

    if (size == 0U) {
        return;
    }
    if (size > WAVE_GRID_WIDTH) {
        size = WAVE_GRID_WIDTH;
    }

    for (row = 0U; row < size; row++) {
        src_row = (uint8_t)(((uint16_t)row * WAVE_GRID_HEIGHT) / size);
        for (col = 0U; col < size; col++) {
            src_col = (uint8_t)(((uint16_t)col * WAVE_GRID_WIDTH) / size);
            if (GetPatternLoadingPixel(frame_index, src_row, src_col) != 0U) {
                OLED_DrawPoint(x + col, y + row);
            }
        }
    }
}

static void PlayPatternProgress(const char *title)
{
    uint16_t percent;
    uint8_t frame_index;

    for (percent = 0U; percent <= 100U; percent++) {
        frame_index = (uint8_t)(((uint16_t)percent * (WAVE_GRID_FRAME_COUNT - 1U)) / 100U);

        OLED_Clear();
        DrawCenteredString(2, title, OLED_6X8_HALF);
        DrawPatternLoadingFrameArea(42, 10, 44U, frame_index);
        DrawCenteredPercent(56, percent);
        OLED_Update();
        Delay_ms(10U);
    }
}

static void ShowPatternWindow(const char *pattern_name)
{
    uint8_t done_action;

    ShowHomingProgress();
    PlayPatternProgress(pattern_name);
    Delay_ms(500U);

    done_action = WaitPatternCompleteAction();
    if (done_action == 0U) {
        PlayPatternProgress("Return");
        Delay_ms(500U);
        OLED_UI_Back();
    } else if (done_action == 1U) {
        OLED_UI_Back();
    }
}

static void ShowCheckerPattern(void)
{
    ShowPatternWindow("Checker");
}

static void ShowBoxPattern(void)
{
    ShowPatternWindow("Box");
}

static void ShowCrossPattern(void)
{
    ShowPatternWindow("Cross");
}

static void ShowSlashPattern(void)
{
    ShowPatternWindow("Slash");
}

static void ShowCube3Pattern(void)
{
    ShowPatternWindow("Cube3");
}

static void ShowFishPattern(void)
{
    ShowPatternWindow("Fish");
}

extern MenuPage PatternMenuPage;
extern MenuPage SettingMenuPage;

MenuItem MainMenuItems[] = {
    {.General_item_text = "Solve Cube", .General_callback = ShowSolveWindow},
    {.General_item_text = "Scramble", .General_callback = ShowScrambleStepSelector},
    {.General_item_text = "Pattern", .General_SubMenuPage = &PatternMenuPage},
    {.General_item_text = "Setting", .General_SubMenuPage = &SettingMenuPage},
    {.General_item_text = NULL},
};

MenuItem PatternMenuItems[] = {
    {.General_item_text = "Checker", .General_callback = ShowCheckerPattern},
    {.General_item_text = "Box", .General_callback = ShowBoxPattern},
    {.General_item_text = "Cross", .General_callback = ShowCrossPattern},
    {.General_item_text = "Slash", .General_callback = ShowSlashPattern},
    {.General_item_text = "Cube3", .General_callback = ShowCube3Pattern},
    {.General_item_text = "Fish", .General_callback = ShowFishPattern},
    {.General_item_text = "Back", .General_callback = OLED_UI_Back},
    {.General_item_text = NULL},
};

MenuItem SettingMenuItems[] = {
    {.General_item_text = "Motor Speed", .General_callback = ShowDashboardWindow},
    {.General_item_text = "Brightness", .General_callback = ShowBrightnessWindow},
    {.General_item_text = "OLED FPS", .General_callback = ShowFPSForFiveSeconds},
    {.General_item_text = "Back", .General_callback = OLED_UI_Back},
    {.General_item_text = NULL},
};

MenuPage MainMenuPage = {
    .General_MenuType = MENU_TYPE_LIST,
    .General_MovingSpeed = 5,
    .General_CursorStyle = REVERSE_ROUNDRECTANGLE,
    .General_MoveStyle = UNLINEAR,
    .General_FontSize = OLED_UI_FONT_12,
    .General_ParentMenuPage = NULL,
    .General_MenuItems = MainMenuItems,
    .General_LineSpace = 8,
    .General_ShowAuxiliaryFunction = NULL,
    .List_MenuArea = {0, 0, 128, 64},
    .List_IfDrawFrame = false,
    .List_IfDrawLinePerfix = true,
    .List_StartPointX = 6,
    .List_StartPointY = 8,
};

MenuPage PatternMenuPage = {
    .General_MenuType = MENU_TYPE_LIST,
    .General_MovingSpeed = 5,
    .General_CursorStyle = REVERSE_ROUNDRECTANGLE,
    .General_MoveStyle = UNLINEAR,
    .General_FontSize = OLED_UI_FONT_12,
    .General_ParentMenuPage = &MainMenuPage,
    .General_MenuItems = PatternMenuItems,
    .General_LineSpace = 8,
    .General_ShowAuxiliaryFunction = NULL,
    .List_MenuArea = {0, 0, 128, 64},
    .List_IfDrawFrame = false,
    .List_IfDrawLinePerfix = true,
    .List_StartPointX = 6,
    .List_StartPointY = 8,
};

MenuPage SettingMenuPage = {
    .General_MenuType = MENU_TYPE_LIST,
    .General_MovingSpeed = 5,
    .General_CursorStyle = REVERSE_ROUNDRECTANGLE,
    .General_MoveStyle = UNLINEAR,
    .General_FontSize = OLED_UI_FONT_12,
    .General_ParentMenuPage = &MainMenuPage,
    .General_MenuItems = SettingMenuItems,
    .General_LineSpace = 8,
    .General_ShowAuxiliaryFunction = DrawTimedFPS,
    .List_MenuArea = {0, 0, 128, 64},
    .List_IfDrawFrame = false,
    .List_IfDrawLinePerfix = true,
    .List_StartPointX = 6,
    .List_StartPointY = 8,
};
