#include "../inc/color_sensor_api.h"
#include "at32f403a_407_board.h"

volatile color_rgb_t g_color_last_rgb[2];
volatile color_hsv_t g_color_last_hsv[2];
volatile int g_color_last_status[2] = {0, 0};
volatile unsigned long g_color_print_count = 0UL;

int color_unit_test_run(void)
{
    static int inited = 0;
    color_rgb_t rgb0 = {0U, 0U, 0U};
    color_rgb_t rgb1 = {0U, 0U, 0U};
    color_hsv_t hsv0;
    color_hsv_t hsv1;
    color_ret_t ret0;
    color_ret_t ret1;
    color_ret_t ret;

    if (!inited) {
        ret = color_sensor_init();
        if (ret != COLOR_RET_OK) {
            g_color_last_status[0] = (int)ret;
            g_color_last_status[1] = (int)ret;
            printf("[COLOR] init failed: %d\r\n", (int)ret);
            return (int)ret;
        }
        inited = 1;
    }

    ret0 = color_sensor_read_rgb_by_id(0U, &rgb0);
    if (ret0 == COLOR_RET_OK) {
        hsv0 = color_rgb_to_hsv(rgb0);
        g_color_last_rgb[0] = rgb0;
        g_color_last_hsv[0] = hsv0;
        g_color_last_status[0] = 0;
    } else {
        g_color_last_status[0] = (int)ret0;
        g_color_last_rgb[0].r = 0U;
        g_color_last_rgb[0].g = 0U;
        g_color_last_rgb[0].b = 0U;
        g_color_last_hsv[0].h = 0U;
        g_color_last_hsv[0].s = 0U;
        g_color_last_hsv[0].v = 0U;
    }

    ret1 = color_sensor_read_rgb_by_id(1U, &rgb1);
    if (ret1 == COLOR_RET_OK) {
        hsv1 = color_rgb_to_hsv(rgb1);
        g_color_last_rgb[1] = rgb1;
        g_color_last_hsv[1] = hsv1;
        g_color_last_status[1] = 0;
    } else {
        g_color_last_status[1] = (int)ret1;
        g_color_last_rgb[1].r = 0U;
        g_color_last_rgb[1].g = 0U;
        g_color_last_rgb[1].b = 0U;
        g_color_last_hsv[1].h = 0U;
        g_color_last_hsv[1].s = 0U;
        g_color_last_hsv[1].v = 0U;
    }

    g_color_print_count++;
    
    /* 單感測器輸出邏輯：優先輸出 S0，如果 S0 失敗則輸出 S1 */
    if (ret0 == COLOR_RET_OK) {
        printf(
            "CS1,SEQ=%lu,SENSOR=0,ST=%d,R=%u,G=%u,B=%u,H=%u,SAT=%u,V=%u\r\n",
            g_color_print_count,
            g_color_last_status[0],
            (unsigned int)g_color_last_rgb[0].r,
            (unsigned int)g_color_last_rgb[0].g,
            (unsigned int)g_color_last_rgb[0].b,
            (unsigned int)g_color_last_hsv[0].h,
            (unsigned int)g_color_last_hsv[0].s,
            (unsigned int)g_color_last_hsv[0].v
        );
        return 0;
    } else if (ret1 == COLOR_RET_OK) {
        printf(
            "CS1,SEQ=%lu,SENSOR=1,ST=%d,R=%u,G=%u,B=%u,H=%u,SAT=%u,V=%u\r\n",
            g_color_print_count,
            g_color_last_status[1],
            (unsigned int)g_color_last_rgb[1].r,
            (unsigned int)g_color_last_rgb[1].g,
            (unsigned int)g_color_last_rgb[1].b,
            (unsigned int)g_color_last_hsv[1].h,
            (unsigned int)g_color_last_hsv[1].s,
            (unsigned int)g_color_last_hsv[1].v
        );
        return 0;
    }

    return (int)ret0;
}
