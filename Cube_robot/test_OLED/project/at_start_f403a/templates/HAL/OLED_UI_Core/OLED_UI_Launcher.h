#ifndef __OLED_UI_LAUNCHER_H
#define __OLED_UI_LAUNCHER_H

#include <stdint.h>

#define HAL_TIM_Base_Start_IT(timer) ((void)0)
#define HAL_GetTick() OLED_UI_GetTick()

void OLED_UI_init(void);
void OLED_UI_start(void);
uint32_t OLED_UI_GetTick(void);
void OLED_UI_Tick(uint32_t elapsed_ms);

#endif
