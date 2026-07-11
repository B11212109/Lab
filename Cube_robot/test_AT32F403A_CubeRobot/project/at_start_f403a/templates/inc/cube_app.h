/* cube_app.h - 公開的魔方解析與執行介面 */
#ifndef CUBE_APP_H
#define CUBE_APP_H

#include <stdint.h>

/* 初始化（目前保留空實作） */
void cube_app_init(void);

/*
 * 依照目前面向（face_on_stepper_2）執行一段求解字串，回傳執行後的 face_on_stepper_2
 * solution 範例: "U R' F2 ..." 或連續字元形式
 */
char cube_app_execute_solution(char face_on_stepper_2, const char *solution);

#endif /* CUBE_APP_H */
