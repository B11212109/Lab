#ifndef __OLED_UI_MENUDATA_H
#define __OLED_UI_MENUDATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "OLED_UI.h"

extern MenuItem MainMenuItems[];
extern MenuPage MainMenuPage;
extern MenuItem PatternMenuItems[];
extern MenuPage PatternMenuPage;
extern MenuItem SettingMenuItems[];
extern MenuPage SettingMenuPage;

uint16_t OLED_UI_MenuData_GetScrambleSteps(void);
void OLED_UI_MenuData_ShowTask(const char *title, uint8_t percent, const char *status);
void OLED_UI_MenuData_ShowTaskFrame(const char *title, uint8_t percent, const char *status,
                                    uint16_t frame_index);
void OLED_UI_MenuData_ShowScrambleStep(uint16_t remaining_steps, uint8_t percent, const char *status);
void OLED_UI_MenuData_ShowScrambleStepFrame(uint16_t remaining_steps, uint8_t percent,
                                            const char *status, uint16_t frame_index);
void OLED_UI_MenuData_ShowPatternProgress(const char *pattern_name, uint8_t percent);
void OLED_UI_MenuData_ShowPatternFrame(const char *pattern_name, uint8_t percent,
                                       uint16_t frame_index);
uint8_t OLED_UI_MenuData_WaitPatternDoneAction(void);
uint8_t OLED_UI_MenuData_WaitScrambleDoneAction(void);
void OLED_UI_MenuData_ShowHomingOnly(void);
void OLED_UI_MenuData_WaitPlaceCube(void);

#ifdef __cplusplus
}
#endif

#endif
