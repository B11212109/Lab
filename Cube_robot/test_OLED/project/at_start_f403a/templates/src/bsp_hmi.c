#include "bsp_hmi.h"

#include "at32f403a_407_adc.h"
#include "at32f403a_407_board.h"
#include "at32f403a_407_crm.h"
#include "at32f403a_407_gpio.h"
#include "at32f403a_407_tmr.h"

#define HMI_JOY_X_PORT                   GPIOB
#define HMI_JOY_X_PIN                    GPIO_PINS_0
#define HMI_JOY_X_GPIO_CLK               CRM_GPIOB_PERIPH_CLOCK
#define HMI_JOY_X_ADC_CHANNEL            ADC_CHANNEL_8

#define HMI_JOY_Y_PORT                   GPIOB
#define HMI_JOY_Y_PIN                    GPIO_PINS_1
#define HMI_JOY_Y_GPIO_CLK               CRM_GPIOB_PERIPH_CLOCK
#define HMI_JOY_Y_ADC_CHANNEL            ADC_CHANNEL_9

#define HMI_ENCODER_CLK_PORT             GPIOA
#define HMI_ENCODER_CLK_PIN              GPIO_PINS_8
#define HMI_ENCODER_DT_PORT              GPIOA
#define HMI_ENCODER_DT_PIN               GPIO_PINS_9
#define HMI_ENCODER_GPIO_CLK             CRM_GPIOA_PERIPH_CLOCK
#define HMI_ENCODER_TMR                  TMR1
#define HMI_ENCODER_TMR_CLK              CRM_TMR1_PERIPH_CLOCK
#define HMI_ENCODER_PERIOD               0xFFFFU
#define HMI_ENCODER_COUNTS_PER_STEP      1
#define HMI_ENCODER_FORWARD_SIGN         1

#define HMI_JOY_ADC                      ADC1
#define HMI_JOY_ADC_CLK                  CRM_ADC1_PERIPH_CLOCK

#define HMI_SW_PORT                      GPIOA
#define HMI_SW_PIN                       GPIO_PINS_5
#define HMI_SW_GPIO_CLK                  CRM_GPIOA_PERIPH_CLOCK

#define HMI_RESET_PORT                   GPIOA
#define HMI_RESET_PIN                    GPIO_PINS_10
#define HMI_RESET_GPIO_CLK               CRM_GPIOA_PERIPH_CLOCK

#define HMI_JOY_CENTER                   2048
#define HMI_JOY_HIGH_THRESHOLD           3000U
#define HMI_JOY_LOW_THRESHOLD            1000U
#define HMI_SW_DEBOUNCE_MS               20U

static uint8_t g_hmi_ready = 0U;
static uint8_t g_sw_pressed_latched = 0U;
static uint8_t g_reset_pressed_latched = 0U;
static hmi_action_t g_axis_repeat_action = ACTION_NONE;
static hmi_action_t g_last_action = ACTION_NONE;
static uint16_t g_encoder_last_count = 0U;
static int16_t g_encoder_value_accum = 0;
static uint8_t g_encoder_percent = 50U;

static uint8_t hmi_sw_is_low(void)
{
    return ((HMI_SW_PORT->idt & HMI_SW_PIN) == 0U) ? 1U : 0U;
}

static uint8_t hmi_reset_is_low(void)
{
    return ((HMI_RESET_PORT->idt & HMI_RESET_PIN) == 0U) ? 1U : 0U;
}

void bsp_hmi_init(void)
{
    gpio_init_type gpio_init_struct;
    adc_base_config_type adc_base_struct;

    crm_periph_clock_enable(HMI_JOY_X_GPIO_CLK, TRUE);
    crm_periph_clock_enable(HMI_JOY_Y_GPIO_CLK, TRUE);
    crm_periph_clock_enable(HMI_ENCODER_GPIO_CLK, TRUE);
    crm_periph_clock_enable(HMI_ENCODER_TMR_CLK, TRUE);
    crm_periph_clock_enable(HMI_SW_GPIO_CLK, TRUE);
    crm_periph_clock_enable(HMI_RESET_GPIO_CLK, TRUE);
    crm_periph_clock_enable(HMI_JOY_ADC_CLK, TRUE);
    crm_adc_clock_div_set(CRM_ADC_DIV_6);

    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init_struct.gpio_mode = GPIO_MODE_ANALOG;
    gpio_init_struct.gpio_pins = HMI_JOY_X_PIN;
    gpio_init(HMI_JOY_X_PORT, &gpio_init_struct);
    gpio_init_struct.gpio_pins = HMI_JOY_Y_PIN;
    gpio_init(HMI_JOY_Y_PORT, &gpio_init_struct);

    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
    gpio_init_struct.gpio_pull = GPIO_PULL_UP;
    gpio_init_struct.gpio_pins = HMI_ENCODER_CLK_PIN | HMI_ENCODER_DT_PIN;
    gpio_init(HMI_ENCODER_CLK_PORT, &gpio_init_struct);

    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_pull = GPIO_PULL_UP;
    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
    gpio_init_struct.gpio_pins = HMI_SW_PIN;
    gpio_init(HMI_SW_PORT, &gpio_init_struct);
    gpio_init_struct.gpio_pins = HMI_RESET_PIN;
    gpio_init(HMI_RESET_PORT, &gpio_init_struct);

    tmr_base_init(HMI_ENCODER_TMR, HMI_ENCODER_PERIOD, 0U);
    tmr_encoder_mode_config(HMI_ENCODER_TMR,
                            TMR_ENCODER_MODE_C,
                            TMR_INPUT_RISING_EDGE,
                            TMR_INPUT_RISING_EDGE);
    tmr_input_channel_filter_set(HMI_ENCODER_TMR, TMR_SELECT_CHANNEL_1, 0x0FU);
    tmr_input_channel_filter_set(HMI_ENCODER_TMR, TMR_SELECT_CHANNEL_2, 0x0FU);
    tmr_counter_value_set(HMI_ENCODER_TMR, HMI_ENCODER_PERIOD / 2U);
    g_encoder_last_count = (uint16_t)tmr_counter_value_get(HMI_ENCODER_TMR);
    tmr_counter_enable(HMI_ENCODER_TMR, TRUE);

    adc_reset(HMI_JOY_ADC);
    adc_combine_mode_select(ADC_INDEPENDENT_MODE);
    adc_base_default_para_init(&adc_base_struct);
    adc_base_struct.sequence_mode = FALSE;
    adc_base_struct.repeat_mode = FALSE;
    adc_base_struct.data_align = ADC_RIGHT_ALIGNMENT;
    adc_base_struct.ordinary_channel_length = 1U;
    adc_base_config(HMI_JOY_ADC, &adc_base_struct);
    adc_ordinary_channel_set(HMI_JOY_ADC,
                             HMI_JOY_Y_ADC_CHANNEL,
                             1U,
                             ADC_SAMPLETIME_239_5);
    adc_ordinary_conversion_trigger_set(HMI_JOY_ADC,
                                        ADC12_ORDINARY_TRIG_SOFTWARE,
                                        TRUE);

    adc_enable(HMI_JOY_ADC, TRUE);
    adc_calibration_init(HMI_JOY_ADC);
    while (adc_calibration_init_status_get(HMI_JOY_ADC) != RESET) {
    }
    adc_calibration_start(HMI_JOY_ADC);
    while (adc_calibration_status_get(HMI_JOY_ADC) != RESET) {
    }

    g_hmi_ready = 1U;
}

static uint16_t hmi_read_adc(adc_channel_select_type channel)
{
    if (g_hmi_ready == 0U) {
        return 0U;
    }

    adc_ordinary_channel_set(HMI_JOY_ADC, channel, 1U, ADC_SAMPLETIME_239_5);
    adc_flag_clear(HMI_JOY_ADC, ADC_CCE_FLAG);
    adc_ordinary_software_trigger_enable(HMI_JOY_ADC, TRUE);
    while (adc_flag_get(HMI_JOY_ADC, ADC_CCE_FLAG) == RESET) {
    }

    return adc_ordinary_conversion_data_get(HMI_JOY_ADC);
}

uint16_t hmi_get_joy_x(void)
{
    return hmi_read_adc(HMI_JOY_X_ADC_CHANNEL);
}

uint16_t hmi_get_joy_y(void)
{
    return hmi_read_adc(HMI_JOY_Y_ADC_CHANNEL);
}

static int16_t hmi_encoder_read_delta(void)
{
    uint16_t current_count;
    int16_t delta;

    current_count = (uint16_t)tmr_counter_value_get(HMI_ENCODER_TMR);
    delta = (int16_t)(current_count - g_encoder_last_count);
    g_encoder_last_count = current_count;
#if HMI_ENCODER_FORWARD_SIGN < 0
    delta = (int16_t)(-delta);
#endif
    return delta;
}

static void hmi_encoder_update(void)
{
    int16_t delta;

    delta = hmi_encoder_read_delta();
    if (delta == 0) {
        return;
    }

    g_encoder_value_accum = (int16_t)(g_encoder_value_accum + delta);

    while (g_encoder_value_accum >= HMI_ENCODER_COUNTS_PER_STEP) {
        if (g_encoder_percent < 100U) {
            g_encoder_percent++;
        }
        g_encoder_value_accum = (int16_t)(g_encoder_value_accum - HMI_ENCODER_COUNTS_PER_STEP);
    }
    while (g_encoder_value_accum <= -HMI_ENCODER_COUNTS_PER_STEP) {
        if (g_encoder_percent > 0U) {
            g_encoder_percent--;
        }
        g_encoder_value_accum = (int16_t)(g_encoder_value_accum + HMI_ENCODER_COUNTS_PER_STEP);
    }
}

uint16_t hmi_get_speed_adc(void)
{
    hmi_encoder_update();

    return (uint16_t)(((uint32_t)g_encoder_percent * 4095U + 50U) / 100U);
}

uint8_t hmi_get_speed_percent(void)
{
    hmi_encoder_update();

    return g_encoder_percent;
}

void hmi_set_speed_percent(uint8_t percent)
{
    if (percent > 100U) {
        percent = 100U;
    }

    hmi_encoder_update();
    g_encoder_percent = percent;
    g_encoder_value_accum = 0;
}

uint8_t hmi_consume_reset_button(void)
{
    if (g_hmi_ready == 0U) {
        return 0U;
    }

    if (hmi_reset_is_low() != 0U) {
        if (g_reset_pressed_latched == 0U) {
            delay_ms(HMI_SW_DEBOUNCE_MS);
            if (hmi_reset_is_low() != 0U) {
                g_reset_pressed_latched = 1U;
                return 1U;
            }
        }
        return 0U;
    }

    g_reset_pressed_latched = 0U;
    return 0U;
}

static int32_t hmi_abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static hmi_action_t hmi_axis_to_action(uint16_t joy_x, uint16_t joy_y)
{
    int32_t x_deflection = hmi_abs_i32((int32_t)joy_x - HMI_JOY_CENTER);
    int32_t y_deflection = hmi_abs_i32((int32_t)joy_y - HMI_JOY_CENTER);
    hmi_action_t x_action = ACTION_NONE;
    hmi_action_t y_action = ACTION_NONE;

    if (joy_x > HMI_JOY_HIGH_THRESHOLD) {
        x_action = ACTION_LEFT;
    } else if (joy_x < HMI_JOY_LOW_THRESHOLD) {
        x_action = ACTION_RIGHT;
    }

    if (joy_y > HMI_JOY_HIGH_THRESHOLD) {
        y_action = ACTION_UP;
    } else if (joy_y < HMI_JOY_LOW_THRESHOLD) {
        y_action = ACTION_DOWN;
    }

    if (x_action == ACTION_NONE && y_action == ACTION_NONE) {
        return ACTION_NONE;
    }

    if (y_action != ACTION_NONE && y_deflection >= x_deflection) {
        return y_action;
    }

    return x_action;
}

hmi_action_t hmi_get_action(void)
{
    hmi_action_t action;
    uint16_t joy_x;
    uint16_t joy_y;

    if (g_hmi_ready == 0U) {
        return ACTION_NONE;
    }

    if (hmi_sw_is_low() != 0U) {
        if (g_sw_pressed_latched == 0U) {
            delay_ms(HMI_SW_DEBOUNCE_MS);
            if (hmi_sw_is_low() != 0U) {
                g_sw_pressed_latched = 1U;
                g_axis_repeat_action = ACTION_NONE;
                g_last_action = ACTION_CLICK;
                return ACTION_CLICK;
            }
        }
        return ACTION_NONE;
    }
    g_sw_pressed_latched = 0U;

    joy_x = hmi_get_joy_x();
    joy_y = hmi_get_joy_y();
    action = hmi_axis_to_action(joy_x, joy_y);
    if (action == ACTION_NONE) {
        g_axis_repeat_action = ACTION_NONE;
        g_last_action = ACTION_NONE;
        return ACTION_NONE;
    }

    if (action != g_axis_repeat_action) {
        g_axis_repeat_action = action;
        g_last_action = action;
        return action;
    }

    return ACTION_NONE;
}

hmi_action_t hmi_get_last_action(void)
{
    return g_last_action;
}
