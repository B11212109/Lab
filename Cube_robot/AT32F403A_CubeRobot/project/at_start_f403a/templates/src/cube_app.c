/* cube_app.c - converts solver text moves to robot motor actions. */
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "cube_app.h"
#include "bsp_stepper.h"

/*
 * Current AT32 axis mapping:
 * - M1/M2 are the two arm motors.
 * - M3 is the base motor handled by action_turn_base().
 * - action_flip_cube_steps() drives both arms together.
 */


static char cube_app_tweak(char face_on_stepper_2, const char *action, const char *action2)
{
    char face = action[0];
    char face_after_tweak = face_on_stepper_2;
    int angle_u = 0, angle_d = 0;
    int angle_lfrb = 0;
    int flip_time = 0;
    assert(face == 'U' || face == 'R' || face == 'F' || face == 'D' || face == 'L' || face == 'B');

    if (face == 'L' || face == 'F' || face == 'R' || face == 'B')
    {
        if (action[1] == '\'')
        {
            angle_lfrb = -800; /* 對 M2 來說，支援負值表示逆轉90度 */
        }
        else if (action[1] == '2')
        {
            angle_lfrb = 1600;
        }
        else if (action[1] == '0')
        {
            angle_lfrb = 0;
        }
        else
        {
            angle_lfrb = 800;
        }
        face_after_tweak = face;

        /* 計算需要呼叫翻轉次數（flip_time），邏輯同原始程式 */
        if (face_on_stepper_2 == 'F')
        {
            if (face_after_tweak == 'F') flip_time = 0;
            else if (face_after_tweak == 'R') flip_time = 1;
            else if (face_after_tweak == 'B') flip_time = 2;
            else if (face_after_tweak == 'L') flip_time = 3;
        }
        else if (face_on_stepper_2 == 'R')
        {
            if (face_after_tweak == 'F') flip_time = 3;
            else if (face_after_tweak == 'R') flip_time = 0;
            else if (face_after_tweak == 'B') flip_time = 1;
            else if (face_after_tweak == 'L') flip_time = 2;
        }
        else if (face_on_stepper_2 == 'B')
        {
            if (face_after_tweak == 'F') flip_time = 2;
            else if (face_after_tweak == 'R') flip_time = 3;
            else if (face_after_tweak == 'B') flip_time = 0;
            else if (face_after_tweak == 'L') flip_time = 1;
        }
        else if (face_on_stepper_2 == 'L')
        {
            if (face_after_tweak == 'F') flip_time = 1;
            else if (face_after_tweak == 'R') flip_time = 2;
            else if (face_after_tweak == 'B') flip_time = 3;
            else if (face_after_tweak == 'L') flip_time = 0;
        }

        /* 執行翻轉：直接呼叫 action_flip_cube() 指定次數（移除原作者雙手協調細節） */
        /* Original logic: flip_cube(flip_time * 800). */
        if (flip_time != 0)
        {
            action_flip_cube_steps(flip_time * 800);
        }

        /* 旋轉 face（由 base motor 處理），M2 支援負數表示逆向 */
        if (angle_lfrb != 0)
        {
            action_turn_base(angle_lfrb);
        }
    }
    else /* U 或 D */
    {
        if (face == 'U')
        {
            if (action[1] == '\'') angle_u = 800;
            else if (action[1] == '2') angle_u = 1600;
            else angle_u = 2400; /* M1/M2 arm pair uses +270 degrees for U. */
        }
        else if (face == 'D')
        {
            if (action[1] == '\'') angle_d = 2400;
            else if (action[1] == '2') angle_d = 1600;
            else angle_d = 800;
        }

        if (action2 != NULL)
        {
            assert(action2[0] == 'U' || action2[0] == 'D');
            if (action2[0] == 'U')
            {
                assert(angle_u == 0);
                if (action2[1] == '\'') angle_u = 800;
                else if (action2[1] == '2') angle_u = 1600;
                else angle_u = 2400;
            }
            else if (action2[0] == 'D')
            {
                assert(angle_d == 0);
                if (action2[1] == '\'') angle_d = 2400;
                else if (action2[1] == '2') angle_d = 1600;
                else angle_d = 800;
            }
        }

        /* 以安全 API 呼叫：M3 -> motor_id 0, M1 -> motor_id 1
         * 兩個動作可以分別呼叫，底層實作可並行或序列化處理
         */
        /* U/D path follows original cube_ud() on the two arm motors. */
        action_turn_ud(angle_u, angle_d);
    }

    return face_after_tweak;
}

char cube_app_execute_solution(char face_on_stepper_2, const char *solution)
{
    /* 將 solution 解析為固定寬度的 p_dst 形式，與原始程式行為相同 */
    char tmp[128];
    char p_dst[128];
    size_t len;
    int count = 0;
    const char *p;
    int i;

    len = solution ? strlen(solution) : 0;
    if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
    memcpy(tmp, solution, len);
    tmp[len] = '\0';

    p = tmp;
    /* 整理格式，每步放兩字元 (face + modifier) */
    while (*p != '\0' && count < 60)
    {
        if (*p == 'U' || *p == 'R' || *p == 'F' || *p == 'D' || *p == 'L' || *p == 'B')
        {
            char f = *p++;
            char m = 0;
            if (*p == '\'') { m = '\''; p++; }
            else if (*p == '2') { m = '2'; p++; }
            else if (*p == '0') { m = '0'; p++; }
            else m = 0;
            p_dst[count * 2] = f;
            p_dst[count * 2 + 1] = m;
            count++;
            continue;
        }
        p++;
    }
    p_dst[count * 2] = 0;

    for (i = 0; i < count;)
    {
        if (stepper_emergency_stop_requested() != 0U) {
            break;
        }

        if (i <= count - 2 &&
            ((p_dst[i * 2] == 'U' && p_dst[i * 2 + 2] == 'D') ||
             (p_dst[i * 2] == 'D' && p_dst[i * 2 + 2] == 'U')))
        {
            face_on_stepper_2 = cube_app_tweak(face_on_stepper_2, &p_dst[i * 2], &p_dst[i * 2 + 2]);
            i += 2;
        }
        else
        {
            face_on_stepper_2 = cube_app_tweak(face_on_stepper_2, &p_dst[i * 2], NULL);
            i += 1;
        }

        if (stepper_emergency_stop_requested() != 0U) {
            break;
        }
    }

    return face_on_stepper_2;
}

void cube_app_init(void)
{
    bsp_stepper_init();
}
