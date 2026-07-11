#include "color_detect.h"

#include <stdint.h>

/*
 * Thresholds tuned from test_Color:
 * - scan_result_cube_sansor0.csv
 * - scan_result_cube_sansor1.csv
 * - scan_result_cube2_sansor1.csv
 *
 * Calibration ranges, non-center stickers:
 * D/white  S=316..403
 * U/yellow S=702..738, H=63..71
 * R/red    H=2..4
 * L/orange H=9..11
 * B/green  H=115..119
 * F/blue   H=202..210
 */
#define COLOR_DARK_V_MAX             30
#define COLOR_DARK_COUNT_MAX         10
#define COLOR_WHITE_S_MAX            520

#define COLOR_RED_H_MAX              6
#define COLOR_ORANGE_H_MIN           7
#define COLOR_ORANGE_H_MAX           25
#define COLOR_YELLOW_H_MIN           45
#define COLOR_YELLOW_H_MAX           95
#define COLOR_GREEN_H_MIN            95
#define COLOR_GREEN_H_MAX            155
#define COLOR_BLUE_H_MIN             170
#define COLOR_BLUE_H_MAX             245

typedef struct {
    char face;
    int h_center;
} hue_center_t;

static int max3(int a, int b, int c)
{
    int value = a;

    if (b > value) {
        value = b;
    }
    if (c > value) {
        value = c;
    }

    return value;
}

static int min3(int a, int b, int c)
{
    int value = a;

    if (b < value) {
        value = b;
    }
    if (c < value) {
        value = c;
    }

    return value;
}

void rgb2hsv(int r, int g, int b, int *h, int *s, int *v)
{
    int mx = max3(r, g, b);
    int mn = min3(r, g, b);
    int delta = mx - mn;

    if (delta == 0) {
        *h = 0;
    } else if (mx == r) {
        if (g >= b) {
            *h = (g - b) * 60 / delta;
        } else {
            *h = (g - b) * 60 / delta + 360;
        }
    } else if (mx == g) {
        *h = (b - r) * 60 / delta + 120;
    } else {
        *h = (r - g) * 60 / delta + 240;
    }

    if (mx == 0) {
        *s = 0;
    } else {
        *s = 1023 * delta / mx;
    }
    *v = mx;
}

static int hue_distance(int a, int b)
{
    int diff = a - b;

    if (diff < 0) {
        diff = -diff;
    }
    if (diff > 180) {
        diff = 360 - diff;
    }

    return diff;
}

static char classify_hsv(int h, int s, int v)
{
    static const hue_center_t centers[] = {
        {'R', 3},
        {'L', 10},
        {'U', 68},
        {'B', 117},
        {'F', 207},
    };
    char best_face = 'R';
    int best_distance = 361;

    (void)v;

    if (s <= COLOR_WHITE_S_MAX) {
        return 'D';
    }

    if (h <= COLOR_RED_H_MAX || h >= 350) {
        return 'R';
    }
    if (h >= COLOR_ORANGE_H_MIN && h <= COLOR_ORANGE_H_MAX) {
        return 'L';
    }
    if (h >= COLOR_YELLOW_H_MIN && h <= COLOR_YELLOW_H_MAX) {
        return 'U';
    }
    if (h >= COLOR_GREEN_H_MIN && h <= COLOR_GREEN_H_MAX) {
        return 'B';
    }
    if (h >= COLOR_BLUE_H_MIN && h <= COLOR_BLUE_H_MAX) {
        return 'F';
    }

    for (uint32_t i = 0; i < (sizeof(centers) / sizeof(centers[0])); i++) {
        int distance = hue_distance(h, centers[i].h_center);
        if (distance < best_distance) {
            best_distance = distance;
            best_face = centers[i].face;
        }
    }

    return best_face;
}

void color_detect(uint16_t *data, char *cube_str)
{
    int dark_count = 0;

    if (data == 0 || cube_str == 0) {
        return;
    }

    for (uint32_t i = 0; i < 54; i++) {
        int h;
        int s;
        int v;

        rgb2hsv(data[i * 3 + 0], data[i * 3 + 1], data[i * 3 + 2], &h, &s, &v);
        data[i * 3 + 0] = (uint16_t)h;
        data[i * 3 + 1] = (uint16_t)s;
        data[i * 3 + 2] = (uint16_t)v;

        if (v < COLOR_DARK_V_MAX) {
            dark_count++;
        }

        cube_str[i] = classify_hsv(h, s, v);
    }

    if (dark_count > COLOR_DARK_COUNT_MAX) {
        cube_str[0] = '\0';
        return;
    }

    cube_str[4] = 'U';
    cube_str[13] = 'R';
    cube_str[22] = 'F';
    cube_str[31] = 'D';
    cube_str[40] = 'L';
    cube_str[49] = 'B';
    cube_str[54] = '\0';
}
