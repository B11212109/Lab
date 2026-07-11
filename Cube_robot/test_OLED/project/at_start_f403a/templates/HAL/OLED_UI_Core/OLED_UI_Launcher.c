#include "OLED_UI_Launcher.h"

#include "OLED_UI.h"
#include "OLED_UI_MenuData.h"

static uint32_t g_oled_ui_tick_ms = 0U;

void OLED_UI_init(void)
{
    OLED_UI_Init(&MainMenuPage);
}

void OLED_UI_start(void)
{
    while (1) {
        OLED_UI_MainLoop();
    }
}

uint32_t OLED_UI_GetTick(void)
{
    return g_oled_ui_tick_ms;
}

void OLED_UI_Tick(uint32_t elapsed_ms)
{
    g_oled_ui_tick_ms += elapsed_ms;
}
