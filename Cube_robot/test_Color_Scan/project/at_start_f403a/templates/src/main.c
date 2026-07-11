/**
  ***************************************************************************
  * @file     main.c
  * @brief    AT32 cube color scan test application
  *
  * Scan colors with the cube robot motion sequence, classify the 54 stickers,
  * and send the classified face string through USART1 PA9/PA10.
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
#include "at32f403a_407_usart.h"
#include "bsp_stepper.h"
#include "color_detect.h"
#include "color_sensor_api.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define BUTTON_PORT                        GPIOA
#define BUTTON_PIN                         GPIO_PINS_5
#define BUTTON_GPIO_CLK                    CRM_GPIOA_PERIPH_CLOCK

#define HALL_PORT                          GPIOB
#define HALL_GPIO_CLK                      CRM_GPIOB_PERIPH_CLOCK
#define HALL_M1_PIN                        GPIO_PINS_5
#define HALL_M2_PIN                        GPIO_PINS_9
#define HALL_M3_PIN                        GPIO_PINS_8

#define HOMING_PREMOVE_STEPS               600
#define HOMING_SEARCH_STEPS                6400
#define HOMING_OFFSETS_M1                  290
#define HOMING_OFFSETS_M2                  210
#define HOMING_OFFSETS_M3                  110
#define HOMING_FIXED_PREMOVE_PPS           1333U
#define HOMING_FIXED_SEARCH_PPS            1333U
#define HOMING_FIXED_OFFSET_PPS            667U

#define UART1_BAUDRATE                     115200U
#define CUBE_FACE_LEN                      54U
#define SCAN_STEP_UNIT                     400U
#define SCAN_ARM_OVERSHOOT_STEP            ACTION_ARM_OVERSHOOT_STEP
#define SCAN_RGB_CHANNELS                  3U
#define COLOR_SENSOR_SWAP_LR               1U
#define ARRAY_LEN(a)                       (sizeof(a) / sizeof((a)[0]))
#define UD(x, y)                           ((((uint32_t)(x)) << 8) | (uint32_t)(y))

typedef struct {
    const char *name;
    int32_t arm_steps;
    int32_t base_after_steps;
    const uint32_t *color_cmd;
    uint32_t color_cmd_count;
} scan_move_entry_t;

static char cube_str[CUBE_FACE_LEN + 1U] = {0};
static uint16_t color_buffer[CUBE_FACE_LEN * SCAN_RGB_CHANNELS] = {0};
static bool g_color_ready = false;

static const uint32_t g_color_cmd_0_270_l[7] = {UD(36, 42), UD(23, 48), UD(38, 44), UD(10, 43), UD(9, 15), UD(50, 21), UD(11, 17)};
static const uint32_t g_color_cmd_1_180_r[5] = {0U, UD(37, 16), UD(33, 27), UD(19, 52), UD(0, 6)};
static const uint32_t g_color_cmd_2_270_f[7] = {0U, 0U, UD(29, 35), UD(46, 25), UD(8, 2), 0U, 0U};
static const uint32_t g_color_cmd_3_180_b[5] = {0U, 0U, UD(18, 51), UD(12, 41), UD(47, 26)};
static const uint32_t g_color_cmd_4_270_r[7] = {0U, 0U, UD(45, 24), UD(39, 14), UD(20, 53), 0U, 0U};
static const uint32_t g_color_cmd_5_180_l[5] = {0U, 0U, 0U, UD(5, 32), 0U};
static const uint32_t g_color_cmd_7_180_b[5] = {0U, UD(3, 30), 0U, UD(7, 28), 0U};
static const uint32_t g_color_cmd_8_180[5] = {0U, 0U, 0U, UD(1, 34), 0U};

static const scan_move_entry_t g_scan_move_table[] = {
    {"0_270_L", +2400, 800, g_color_cmd_0_270_l, ARRAY_LEN(g_color_cmd_0_270_l)},
    {"1_180_R", +1600, 800, g_color_cmd_1_180_r, ARRAY_LEN(g_color_cmd_1_180_r)},
    {"2_270_F", +2400, 800, g_color_cmd_2_270_f, ARRAY_LEN(g_color_cmd_2_270_f)},
    {"3_180_B", +1600, 800, g_color_cmd_3_180_b, ARRAY_LEN(g_color_cmd_3_180_b)},
    {"4_270_R", +2400, 800, g_color_cmd_4_270_r, ARRAY_LEN(g_color_cmd_4_270_r)},
    {"5_180_L", +1600, 800, g_color_cmd_5_180_l, ARRAY_LEN(g_color_cmd_5_180_l)},
    {"6_090_F",  +800, 800, NULL, 0U},
    {"7_180_B", +1600, 800, g_color_cmd_7_180_b, ARRAY_LEN(g_color_cmd_7_180_b)},
    {"8_180_X", +1600,   0, g_color_cmd_8_180, ARRAY_LEN(g_color_cmd_8_180)}
};

static void uart1_init(void);
static void uart1_putc(char ch);
static void uart1_puts(const char *text);
static void uart1_put_u32(uint32_t value);
static void uart1_put_i32(int32_t value);
static bool capture_color_pair(uint32_t cmd);
static bool scan_arm_pair(int32_t steps, const uint32_t *color_cmd, size_t color_cmd_count);
static void print_scan_color_summary(const char *face_54);
static void print_scan_hsv_samples(const char *face_54);
static void print_machine_reference(void);
static void print_scan_move_table(void);
static void store_color_sample(uint8_t index, const color_rgb_t *rgb);

static void uart1_init(void)
{
    gpio_init_type gpio_init_struct;

    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_USART1_PERIPH_CLOCK, TRUE);

    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins = GPIO_PINS_9;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOA, &gpio_init_struct);

    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
    gpio_init_struct.gpio_pins = GPIO_PINS_10;
    gpio_init_struct.gpio_pull = GPIO_PULL_UP;
    gpio_init(GPIOA, &gpio_init_struct);

    usart_reset(USART1);
    usart_init(USART1, UART1_BAUDRATE, USART_DATA_8BITS, USART_STOP_1_BIT);
    usart_parity_selection_config(USART1, USART_PARITY_NONE);
    usart_transmitter_enable(USART1, TRUE);
    usart_receiver_enable(USART1, FALSE);
    usart_enable(USART1, TRUE);
}

static void uart1_putc(char ch)
{
    while (usart_flag_get(USART1, USART_TDBE_FLAG) == RESET) {
    }
    usart_data_transmit(USART1, (uint16_t)ch);
}

static void uart1_puts(const char *text)
{
    if (text == NULL) {
        return;
    }

    while (*text != '\0') {
        uart1_putc(*text++);
    }
}

static void uart1_put_u32(uint32_t value)
{
    char digits[11];
    uint32_t index = 0U;

    if (value == 0U) {
        uart1_putc('0');
        return;
    }

    while ((value > 0U) && (index < sizeof(digits))) {
        digits[index++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (index > 0U) {
        uart1_putc(digits[--index]);
    }
}

static void uart1_put_i32(int32_t value)
{
    if (value < 0) {
        uart1_putc('-');
        uart1_put_u32((uint32_t)(-value));
    } else {
        uart1_put_u32((uint32_t)value);
    }
}

static bool hall_triggered(uint16_t pin)
{
    return ((HALL_PORT->idt & pin) == 0U);
}

static bool button_pressed(void)
{
    return ((BUTTON_PORT->idt & BUTTON_PIN) == 0U);
}

static void wait_button_release(void)
{
    while (button_pressed()) {
        delay_ms(10);
    }
}

static void io_init(void)
{
    gpio_init_type gpio_init_struct;

    crm_periph_clock_enable(BUTTON_GPIO_CLK, TRUE);
    crm_periph_clock_enable(HALL_GPIO_CLK, TRUE);

    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_pull = GPIO_PULL_UP;
    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;

    gpio_init_struct.gpio_pins = BUTTON_PIN;
    gpio_init(BUTTON_PORT, &gpio_init_struct);

    gpio_init_struct.gpio_pins = HALL_M1_PIN | HALL_M2_PIN | HALL_M3_PIN;
    gpio_init(HALL_PORT, &gpio_init_struct);
}

static void home_all_motors(void)
{
    const uint16_t home_speed = HOMING_FIXED_SEARCH_PPS;
    const uint16_t offset_speed = HOMING_FIXED_OFFSET_PPS;
    const uint16_t prehome_speed = HOMING_FIXED_PREMOVE_PPS;
    bool homed[STEPPER_MOTOR_COUNT] = { false, false, false };

    stepper_set_enable(1U);

    motor_start_steps(STEPPER_M1_ID, +HOMING_PREMOVE_STEPS, prehome_speed);
    motor_start_steps(STEPPER_M2_ID, +HOMING_PREMOVE_STEPS, prehome_speed);
    motor_start_steps(STEPPER_M3_ID, +HOMING_PREMOVE_STEPS, prehome_speed);
    stepper_wait_all_done();

    motor_start_steps(STEPPER_M1_ID, +HOMING_SEARCH_STEPS, home_speed);
    motor_start_steps(STEPPER_M2_ID, +HOMING_SEARCH_STEPS, home_speed);
    motor_start_steps(STEPPER_M3_ID, +HOMING_SEARCH_STEPS, home_speed);

    while (!(homed[0] && homed[1] && homed[2])) {
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
    }

    motor_start_steps(STEPPER_M1_ID, +HOMING_OFFSETS_M1, offset_speed);
    motor_start_steps(STEPPER_M2_ID, +HOMING_OFFSETS_M2, offset_speed);
    motor_start_steps(STEPPER_M3_ID, +HOMING_OFFSETS_M3, offset_speed);
    stepper_wait_all_done();
}

static bool cube_scan_face_54(char *out_face_54, size_t out_size)
{
    if (!g_color_ready || out_face_54 == NULL || out_size < (CUBE_FACE_LEN + 1U)) {
        if (out_face_54 != NULL && out_size > 0U) {
            out_face_54[0] = '\0';
        }
        return false;
    }

    memset(color_buffer, 0, sizeof(color_buffer));

    for (uint32_t i = 0U; i < ARRAY_LEN(g_scan_move_table); i++) {
        const scan_move_entry_t *move = &g_scan_move_table[i];
        if (!scan_arm_pair(move->arm_steps, move->color_cmd, move->color_cmd_count)) {
            return false;
        }
        if (move->base_after_steps != 0) {
            action_turn_base(move->base_after_steps);
        }
    }

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
    }

    while ((color_cmd != NULL) && (next_sample < color_cmd_count)) {
        if (!capture_color_pair(color_cmd[next_sample])) {
            return false;
        }
        next_sample++;
    }

    if (SCAN_ARM_OVERSHOOT_STEP > 0U) {
        motor_start_steps(STEPPER_M1_ID, dir_sign * (int32_t)SCAN_ARM_OVERSHOOT_STEP, ACTION_SPEED_ARM_PPS);
        motor_start_steps(STEPPER_M2_ID, -dir_sign * (int32_t)SCAN_ARM_OVERSHOOT_STEP, ACTION_SPEED_ARM_PPS);
        stepper_wait_all_done();

        motor_start_steps(STEPPER_M1_ID, -dir_sign * (int32_t)SCAN_ARM_OVERSHOOT_STEP, ACTION_SPEED_ARM_PPS);
        motor_start_steps(STEPPER_M2_ID, dir_sign * (int32_t)SCAN_ARM_OVERSHOOT_STEP, ACTION_SPEED_ARM_PPS);
        stepper_wait_all_done();
    }

    return true;
}

static uint32_t cube_color_count_index(char color)
{
    switch (color) {
    case 'U': return 0U;
    case 'R': return 1U;
    case 'F': return 2U;
    case 'D': return 3U;
    case 'L': return 4U;
    case 'B': return 5U;
    default:  return 6U;
    }
}

static void print_face_string(const char *face_54, uint32_t face_index)
{
    for (uint32_t pos = 0U; pos < 9U; pos++) {
        char color = face_54[(face_index * 9U) + pos];
        uart1_putc((color == '\0') ? '?' : color);
    }
}

static void print_scan_color_summary(const char *face_54)
{
    static const char face_names[6] = {'U', 'R', 'F', 'D', 'L', 'B'};
    uint32_t counts[7] = {0U, 0U, 0U, 0U, 0U, 0U, 0U};
    bool count_ok = true;

    if (face_54 == NULL) {
        return;
    }

    for (uint32_t i = 0U; i < CUBE_FACE_LEN; i++) {
        counts[cube_color_count_index(face_54[i])]++;
    }

    uart1_puts("LEGEND U=YELLOW R=RED F=BLUE D=WHITE L=ORANGE B=GREEN\r\n");
    uart1_puts("COLORS_BY_FACE\r\n");
    for (uint32_t face = 0U; face < 6U; face++) {
        uart1_puts("FACE_");
        uart1_putc(face_names[face]);
        uart1_putc('=');
        print_face_string(face_54, face);
        uart1_puts("\r\n");
    }

    uart1_puts("FACE_ROWS_BEGIN\r\n");
    for (uint32_t face = 0U; face < 6U; face++) {
        for (uint32_t row = 0U; row < 3U; row++) {
            uart1_putc(face_names[face]);
            uart1_putc((char)('0' + row));
            uart1_putc('=');
            for (uint32_t col = 0U; col < 3U; col++) {
                char color = face_54[(face * 9U) + (row * 3U) + col];
                uart1_putc((color == '\0') ? '?' : color);
            }
            uart1_puts("\r\n");
        }
    }
    uart1_puts("FACE_ROWS_END\r\n");

    uart1_puts("COUNT U=");
    uart1_put_u32(counts[0]);
    uart1_puts(" R=");
    uart1_put_u32(counts[1]);
    uart1_puts(" F=");
    uart1_put_u32(counts[2]);
    uart1_puts(" D=");
    uart1_put_u32(counts[3]);
    uart1_puts(" L=");
    uart1_put_u32(counts[4]);
    uart1_puts(" B=");
    uart1_put_u32(counts[5]);
    uart1_puts(" OTHER=");
    uart1_put_u32(counts[6]);
    uart1_puts("\r\n");

    for (uint32_t i = 0U; i < 6U; i++) {
        if (counts[i] != 9U) {
            count_ok = false;
        }
    }
    if (counts[6] != 0U) {
        count_ok = false;
    }

    uart1_puts("COUNT_OK=");
    uart1_puts(count_ok ? "YES\r\n" : "NO\r\n");
}

static void print_scan_hsv_samples(const char *face_54)
{
    if (face_54 == NULL) {
        return;
    }

    uart1_puts("HSV_BEGIN\r\n");
    for (uint32_t i = 0U; i < CUBE_FACE_LEN; i++) {
        uart1_puts("HSV_SAMPLE INDEX=");
        uart1_put_u32(i);
        uart1_puts(" FACE=");
        uart1_putc(face_54[i]);
        uart1_puts(" H=");
        uart1_put_u32(color_buffer[(i * SCAN_RGB_CHANNELS) + 0U]);
        uart1_puts(" S=");
        uart1_put_u32(color_buffer[(i * SCAN_RGB_CHANNELS) + 1U]);
        uart1_puts(" V=");
        uart1_put_u32(color_buffer[(i * SCAN_RGB_CHANNELS) + 2U]);
        uart1_puts("\r\n");
    }
    uart1_puts("HSV_END\r\n");
}

static void print_machine_reference(void)
{
    uart1_puts("CENTER_REF M1=U/YELLOW M2=D/WHITE M3=F/BLUE\r\n");
    uart1_puts("MOVE_AXIS ARM_PAIR=M1/M2 BASE=M3 SAMPLE_STEP=400\r\n");
#if COLOR_SENSOR_SWAP_LR
    uart1_puts("SENSOR_PAIR_MAP UD_HIGH=SENSOR1 UD_LOW=SENSOR0\r\n");
#else
    uart1_puts("SENSOR_PAIR_MAP UD_HIGH=SENSOR0 UD_LOW=SENSOR1\r\n");
#endif
}

static void print_scan_sample_cmd(uint32_t cmd)
{
    if (cmd == 0U) {
        uart1_puts("--");
        return;
    }

    uart1_put_u32((cmd >> 8) & 0xFFU);
    uart1_putc('/');
    uart1_put_u32(cmd & 0xFFU);
}

static void print_scan_move_table(void)
{
    uart1_puts("MOVE_TABLE_BEGIN\r\n");
    uart1_puts("MOVE_TABLE_NOTE SOURCE=AUTHOR_CUBE_GET_COLOR ARM_PAIR=M1/M2 BASE=M3\r\n");
    for (uint32_t i = 0U; i < ARRAY_LEN(g_scan_move_table); i++) {
        const scan_move_entry_t *move = &g_scan_move_table[i];

        uart1_puts("MOVE ");
        uart1_put_u32(i);
        uart1_puts(" NAME=");
        uart1_puts(move->name);
        uart1_puts(" ARM=");
        uart1_put_i32(move->arm_steps);
        uart1_puts(" BASE_AFTER=");
        uart1_put_i32(move->base_after_steps);
        uart1_puts(" SAMPLES=");
        uart1_put_u32(move->color_cmd_count);
        uart1_puts(" CMD=");

        for (uint32_t sample = 0U; sample < move->color_cmd_count; sample++) {
            if (sample > 0U) {
                uart1_putc(',');
            }
            print_scan_sample_cmd(move->color_cmd[sample]);
        }

        uart1_puts("\r\n");
    }
    uart1_puts("MOVE_TABLE_END\r\n");
}

static void run_scan_cycle(void)
{
    uart1_puts("SCAN_START\r\n");

    if (!cube_scan_face_54(cube_str, sizeof(cube_str))) {
        uart1_puts("SCAN_FAIL\r\n");
        return;
    }

    uart1_puts("SCAN_OK\r\n");
    uart1_puts("FACE54=");
    uart1_puts(cube_str);
    uart1_puts("\r\nLEN=");
    uart1_put_u32((uint32_t)strlen(cube_str));
    uart1_puts("\r\n");
    print_machine_reference();
    print_scan_hsv_samples(cube_str);
    print_scan_color_summary(cube_str);
    print_scan_move_table();
}

int main(void)
{
    color_ret_t color_init_result;

    system_clock_config();
    delay_init();
    uart1_init();
    io_init();
    bsp_stepper_init();

    uart1_puts("\r\nCOLOR_SCAN_BOOT\r\n");
    print_machine_reference();

    color_init_result = color_sensor_init();
    g_color_ready = (color_init_result == COLOR_RET_OK);
    if (g_color_ready) {
        uart1_puts("COLOR_READY\r\n");
    } else {
        uart1_puts("COLOR_INIT_FAIL=");
        uart1_put_i32((int32_t)color_init_result);
        uart1_puts("\r\n");
    }

    uart1_puts("HOMING_START\r\n");
    home_all_motors();
    uart1_puts("HOMING_DONE\r\n");
    uart1_puts("PRESS_JOY_SW_TO_SCAN\r\n");

    while (1) {
        if (button_pressed()) {
            delay_ms(30);
            if (button_pressed()) {
                wait_button_release();
                run_scan_cycle();
                uart1_puts("PRESS_JOY_SW_TO_SCAN\r\n");
            }
        }

        delay_ms(20);
    }
}
