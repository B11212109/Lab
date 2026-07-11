#include "OLED_UI_Driver.h"

#include "at32f403a_407_board.h"
#include "bsp_hmi.h"

static hmi_action_t g_cached_action = ACTION_NONE;
static uint8_t g_cache_valid = 0U;

static hmi_action_t oled_ui_read_action(void)
{
    if (g_cache_valid == 0U) {
        g_cached_action = hmi_get_action();
        g_cache_valid = 1U;
    }
    return g_cached_action;
}

static void oled_ui_finish_key_scan(void)
{
    g_cache_valid = 0U;
}

uint8_t Key_GetEnterStatus(void)
{
    hmi_action_t action = oled_ui_read_action();
    return (action == ACTION_CLICK) ? 0U : 1U;
}

uint8_t Key_GetBackStatus(void)
{
    hmi_action_t action = oled_ui_read_action();
    (void)action;
    return 1U;
}

uint8_t Key_GetUpStatus(void)
{
    hmi_action_t action = oled_ui_read_action();
    return (action == ACTION_UP) ? 0U : 1U;
}

uint8_t Key_GetDownStatus(void)
{
    hmi_action_t action = oled_ui_read_action();
    uint8_t status = (action == ACTION_DOWN) ? 0U : 1U;
    oled_ui_finish_key_scan();
    return status;
}

void Timer_Init(void)
{
}

void Key_Init(void)
{
    bsp_hmi_init();
}

void Encoder_Init(void)
{
}

void Encoder_Enable(void)
{
}

void Encoder_Disable(void)
{
}

int16_t Encoder_Get(void)
{
    return 0;
}

void Delay_ms(uint32_t xms)
{
    delay_ms(xms);
}

void Delay_s(uint32_t xs)
{
    while (xs-- > 0U) {
        Delay_ms(1000U);
    }
}
