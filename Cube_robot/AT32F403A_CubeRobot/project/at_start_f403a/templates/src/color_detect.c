#include "color_detect.h"
#include "solve.h"

#include <stdint.h>

#define COLOR_DARK_V_MAX             30
#define COLOR_DARK_COUNT_MAX         10
#define COLOR_HUE_SHIFT              281
#define COLOR_SAMPLE_COUNT           54U
#define COLOR_RGB_CHANNELS           3U
#define COLOR_REFINE_WINDOW          3U

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

static uint8_t color_is_center_index(uint8_t index)
{
    return (index == 4U || index == 13U || index == 22U ||
            index == 31U || index == 40U || index == 49U);
}

static uint8_t color_cube_str_valid(const char *cube_str)
{
    cube_t cube;

    return cube_from_face_54_checked(&cube, cube_str);
}

static uint8_t color_try_swap_if_valid(char *cube_str, uint8_t index_a, uint8_t index_b)
{
    char tmp;

    if (index_a == index_b || color_is_center_index(index_a) != 0U ||
        color_is_center_index(index_b) != 0U) {
        return 0U;
    }
    if (cube_str[index_a] == cube_str[index_b]) {
        return 0U;
    }

    tmp = cube_str[index_a];
    cube_str[index_a] = cube_str[index_b];
    cube_str[index_b] = tmp;

    if (color_cube_str_valid(cube_str) != 0U) {
        return 1U;
    }

    tmp = cube_str[index_a];
    cube_str[index_a] = cube_str[index_b];
    cube_str[index_b] = tmp;
    return 0U;
}

static void color_refine_boundary_swaps(const uint8_t *index, char *cube_str)
{
    static const uint8_t boundaries[] = {8U, 16U, 24U, 32U, 40U};
    uint32_t b;
    uint32_t distance;
    uint32_t left_offset;

    if (color_cube_str_valid(cube_str) != 0U) {
        return;
    }

    for (distance = 0U; distance < COLOR_REFINE_WINDOW; distance++) {
        for (b = 0U; b < (sizeof(boundaries) / sizeof(boundaries[0])); b++) {
            uint8_t boundary = boundaries[b];
            for (left_offset = 0U; left_offset <= distance; left_offset++) {
                uint32_t right_offset = distance - left_offset;
                uint32_t left_rank = (uint32_t)boundary - 1U - left_offset;
                uint32_t right_rank = (uint32_t)boundary + right_offset;

                if (right_rank >= 48U) {
                    continue;
                }
                if (color_try_swap_if_valid(cube_str,
                                            index[left_rank],
                                            index[right_rank]) != 0U) {
                    return;
                }
            }
        }
    }
}

void color_detect(uint16_t *data, char *cube_str)
{
    int dark_count = 0;
    uint8_t index[COLOR_SAMPLE_COUNT];
    uint32_t i;
    uint32_t j;

    if (data == 0 || cube_str == 0) {
        return;
    }

    for (i = 0U; i < COLOR_SAMPLE_COUNT; i++) {
        int h;
        int s;
        int v;

        rgb2hsv(data[i * COLOR_RGB_CHANNELS + 0U],
                data[i * COLOR_RGB_CHANNELS + 1U],
                data[i * COLOR_RGB_CHANNELS + 2U],
                &h,
                &s,
                &v);
        h -= COLOR_HUE_SHIFT;
        if (h < 0) {
            h += 360;
        }

        data[i * COLOR_RGB_CHANNELS + 0U] = (uint16_t)h;
        data[i * COLOR_RGB_CHANNELS + 1U] = (uint16_t)s;
        data[i * COLOR_RGB_CHANNELS + 2U] = (uint16_t)v;
        index[i] = (uint8_t)i;

        if (v < COLOR_DARK_V_MAX) {
            dark_count++;
        }
    }

    if (dark_count > COLOR_DARK_COUNT_MAX) {
        cube_str[0] = '\0';
        return;
    }

    for (i = 0U; i < 14U; i++) {
        for (j = 0U; j < (COLOR_SAMPLE_COUNT - i - 1U); j++) {
            if (data[(uint32_t)index[j] * COLOR_RGB_CHANNELS + 1U] <
                data[(uint32_t)index[j + 1U] * COLOR_RGB_CHANNELS + 1U]) {
                uint8_t tmp = index[j];
                index[j] = index[j + 1U];
                index[j + 1U] = tmp;
            }
        }
    }

    for (i = 0U; i < 39U; i++) {
        for (j = 0U; j < (40U - i - 1U); j++) {
            if (data[(uint32_t)index[j] * COLOR_RGB_CHANNELS + 0U] <
                data[(uint32_t)index[j + 1U] * COLOR_RGB_CHANNELS + 0U]) {
                uint8_t tmp = index[j];
                index[j] = index[j + 1U];
                index[j + 1U] = tmp;
            }
        }
    }

    for (i = 0U; i < 8U; i++) {
        cube_str[index[i]] = 'F';
    }
    for (i = 8U; i < 16U; i++) {
        cube_str[index[i]] = 'B';
    }
    for (i = 16U; i < 24U; i++) {
        cube_str[index[i]] = 'U';
    }
    for (i = 24U; i < 32U; i++) {
        cube_str[index[i]] = 'L';
    }
    for (i = 32U; i < 40U; i++) {
        cube_str[index[i]] = 'R';
    }
    for (i = 40U; i < 48U; i++) {
        cube_str[index[i]] = 'D';
    }

    cube_str[4] = 'U';
    cube_str[13] = 'R';
    cube_str[22] = 'F';
    cube_str[31] = 'D';
    cube_str[40] = 'L';
    cube_str[49] = 'B';
    cube_str[54] = '\0';

    color_refine_boundary_swaps(index, cube_str);
}
