#ifndef __OLED_UI_DRIVER_H
#define __OLED_UI_DRIVER_H

#include <stdint.h>

uint8_t Key_GetEnterStatus(void);
uint8_t Key_GetBackStatus(void);
uint8_t Key_GetUpStatus(void);
uint8_t Key_GetDownStatus(void);

void Timer_Init(void);
void Key_Init(void);
void Encoder_Init(void);
void Encoder_Enable(void);
void Encoder_Disable(void);
int16_t Encoder_Get(void);
void Delay_ms(uint32_t xms);
void Delay_s(uint32_t xs);

#endif
