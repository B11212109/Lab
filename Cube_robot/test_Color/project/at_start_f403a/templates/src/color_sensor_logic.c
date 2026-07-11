#include "../inc/color_sensor_api.h"
#include "../inc/color_sensor_cfg.h"
#include "../inc/color_sensor_port.h"
#include <stdio.h>
#include <string.h>

extern volatile int g_color_i2c_last_sensor;
extern volatile int g_color_i2c_last_step;
extern volatile unsigned long g_color_i2c_last_flag;

#define CUBE_SAMPLE_COUNT 54U

/* Tuned from current on-board measurements (sensor0). */
#define COLOR_V_DARK_THRESHOLD          30U
#define COLOR_V_MIN_VALID               40U
#define COLOR_S_WHITE_MAX               520U
#define COLOR_V_WHITE_MIN               120U

#define H_RG_SPLIT_MAX                  25U
#define H_YELLOW_MAX                    95U
#define H_GREEN_MAX                     175U
#define H_BLUE_MAX                      270U

/* From logs: orange has much higher G/R than red in low-h region. */
#define ORANGE_GR_RATIO_X1000_MIN       260U

typedef struct {
    uint16_t h;
    uint16_t s;
    uint16_t v;
} hsv_internal_t;

static uint8_t g_sensor_ready[COLOR_SENSOR_COUNT] = {0U};

static uint16_t max3(uint16_t a, uint16_t b, uint16_t c)
{
    uint16_t m = a;
    if (b > m) {
        m = b;
    }
    if (c > m) {
        m = c;
    }
    return m;
}

static uint16_t min3(uint16_t a, uint16_t b, uint16_t c)
{
    uint16_t m = a;
    if (b < m) {
        m = b;
    }
    if (c < m) {
        m = c;
    }
    return m;
}

color_hsv_t color_rgb_to_hsv(color_rgb_t rgb)
{
    color_hsv_t out;
    uint16_t mx;
    uint16_t mn;
    uint16_t diff;
    int32_t hue_tmp;
    int32_t rr;
    int32_t gg;
    int32_t bb;
    int32_t dd;

    out.h = 0U;
    out.s = 0U;
    out.v = 0U;

    mx = max3(rgb.r, rgb.g, rgb.b);
    mn = min3(rgb.r, rgb.g, rgb.b);
    diff = (uint16_t)(mx - mn);
    hue_tmp = 0;
    rr = (int32_t)rgb.r;
    gg = (int32_t)rgb.g;
    bb = (int32_t)rgb.b;
    dd = (int32_t)diff;

    if (diff == 0U) {
        out.h = 0U;
    } else if (mx == rgb.r) {
        hue_tmp = (60 * (gg - bb)) / dd;
        if (hue_tmp < 0) {
            hue_tmp += 360;
        }
        out.h = (uint16_t)hue_tmp;
    } else if (mx == rgb.g) {
        out.h = (uint16_t)((60 * (bb - rr)) / dd + 120);
    } else {
        out.h = (uint16_t)((60 * (rr - gg)) / dd + 240);
    }

    if (mx == 0U) {
        out.s = 0U;
    } else {
        out.s = (uint16_t)((1023UL * (unsigned long)diff) / (unsigned long)mx);
    }
    out.v = mx;
    return out;
}

cube_face_t get_color(color_rgb_t rgb)
{
    uint16_t h;
    unsigned long gr_ratio_x1000;
    color_hsv_t hsv = color_rgb_to_hsv(rgb);

    if (hsv.v < COLOR_V_DARK_THRESHOLD) {
        return FACE_UNKNOWN;
    }

    /* White should be decided before hue slicing. */
    if ((hsv.s <= COLOR_S_WHITE_MAX) && (hsv.v >= COLOR_V_WHITE_MIN)) {
        return FACE_D;
    }

    if (hsv.v < COLOR_V_MIN_VALID) {
        return FACE_UNKNOWN;
    }

    h = (uint16_t)((hsv.h + 360U - 281U) % 360U);

    if (h <= H_RG_SPLIT_MAX) {
        if (rgb.r == 0U) {
            return FACE_UNKNOWN;
        }
        gr_ratio_x1000 = ((unsigned long)rgb.g * 1000UL) / (unsigned long)rgb.r;
        if (gr_ratio_x1000 >= ORANGE_GR_RATIO_X1000_MIN) {
            return FACE_L;
        }
        return FACE_R;
    }

    if (h <= H_YELLOW_MAX) {
        return FACE_U;
    }
    if (h <= H_GREEN_MAX) {
        return FACE_B;
    }
    if (h <= H_BLUE_MAX) {
        return FACE_F;
    }

    return FACE_UNKNOWN;
}

static color_ret_t color_sensor_init_one(unsigned int sensor_id)
{
    uint8_t cmd;
    uint8_t id;
    color_ret_t ret;

    cmd = (uint8_t)(TCS3472_CMD_BIT | TCS3472_REG_ID);
    ret = color_port_i2c_write(sensor_id, TCS3472_I2C_ADDR_7BIT, &cmd, 1U);
    if (ret != COLOR_RET_OK) {
        return ret;
    }

    ret = color_port_i2c_read(sensor_id, TCS3472_I2C_ADDR_7BIT, &id, 1U);
    if (ret != COLOR_RET_OK) {
        return ret;
    }

    if (id != TCS3472_ID_EXPECTED) {
        return COLOR_RET_BAD_ID;
    }

    ret = color_port_i2c_write_reg(sensor_id,
                                   TCS3472_I2C_ADDR_7BIT,
                                   (uint8_t)(TCS3472_CMD_BIT | TCS3472_REG_ATIME),
                                   TCS3472_ATIME_DEFAULT);
    if (ret != COLOR_RET_OK) {
        return ret;
    }

    ret = color_port_i2c_write_reg(sensor_id,
                                   TCS3472_I2C_ADDR_7BIT,
                                   (uint8_t)(TCS3472_CMD_BIT | TCS3472_REG_WTIME),
                                   TCS3472_WTIME_DEFAULT);
    if (ret != COLOR_RET_OK) {
        return ret;
    }

    ret = color_port_i2c_write_reg(sensor_id,
                                   TCS3472_I2C_ADDR_7BIT,
                                   (uint8_t)(TCS3472_CMD_BIT | TCS3472_REG_CONTROL),
                                   TCS3472_CONTROL_DEFAULT);
    if (ret != COLOR_RET_OK) {
        return ret;
    }

    ret = color_port_i2c_write_reg(sensor_id,
                                   TCS3472_I2C_ADDR_7BIT,
                                   (uint8_t)(TCS3472_CMD_BIT | TCS3472_REG_ENABLE),
                                   TCS3472_ENABLE_PON);
    if (ret != COLOR_RET_OK) {
        return ret;
    }

    color_port_delay_ms(3U);

    ret = color_port_i2c_write_reg(sensor_id,
                                   TCS3472_I2C_ADDR_7BIT,
                                   (uint8_t)(TCS3472_CMD_BIT | TCS3472_REG_ENABLE),
                                   (uint8_t)(TCS3472_ENABLE_PON | TCS3472_ENABLE_AEN));
    return ret;
}

color_ret_t color_sensor_init(void)
{
    color_ret_t ret;
    color_ret_t last_err;
    unsigned int i;
    unsigned int ready_count;

    ret = color_port_i2c_init_all();
    if (ret != COLOR_RET_OK) {
        return ret;
    }

    for (i = 0U; i < COLOR_SENSOR_COUNT; ++i) {
        g_sensor_ready[i] = 0U;
    }
    ready_count = 0U;
    last_err = COLOR_RET_NOT_READY;

    for (i = 0U; i < COLOR_SENSOR_COUNT; ++i) {
        ret = color_sensor_init_one(i);
        if (ret != COLOR_RET_OK) {
            printf("[COLOR] sensor%u not ready ret=%d diag(s=%d step=%d flag=0x%08lX)\r\n",
                   i,
                   (int)ret,
                   g_color_i2c_last_sensor,
                   g_color_i2c_last_step,
                   (unsigned long)g_color_i2c_last_flag);
            last_err = ret;
            continue;
        }

        g_sensor_ready[i] = 1U;
        ready_count++;
    }

    if (ready_count == 0U) {
        return last_err;
    }

    return COLOR_RET_OK;
}

color_ret_t color_sensor_read_rgb_by_id(unsigned int sensor_id, color_rgb_t *out_rgb)
{
    uint8_t raw[6];
    uint8_t cmd;
    color_ret_t ret;

    if (sensor_id >= COLOR_SENSOR_COUNT) {
        return COLOR_RET_INVALID_ARG;
    }

    if (out_rgb == 0) {
        return COLOR_RET_INVALID_ARG;
    }

    if (g_sensor_ready[sensor_id] == 0U) {
        return COLOR_RET_NOT_READY;
    }

    cmd = (uint8_t)(TCS3472_CMD_BIT | TCS3472_CMD_AUTO_INC | TCS3472_REG_RDATAL);
    ret = color_port_i2c_read_regs(sensor_id, TCS3472_I2C_ADDR_7BIT, cmd, raw, 6U);
    if (ret != COLOR_RET_OK) {
        return ret;
    }

    out_rgb->r = (uint16_t)((uint16_t)raw[0] | ((uint16_t)raw[1] << 8));
    out_rgb->g = (uint16_t)((uint16_t)raw[2] | ((uint16_t)raw[3] << 8));
    out_rgb->b = (uint16_t)((uint16_t)raw[4] | ((uint16_t)raw[5] << 8));
    return COLOR_RET_OK;
}

color_ret_t color_sensor_read_rgb(color_rgb_t *out_rgb)
{
    return color_sensor_read_rgb_by_id(0U, out_rgb);
}

static void sort_s_desc_then_h_desc(const hsv_internal_t *hsv, uint8_t *index)
{
    unsigned int i;
    unsigned int j;

    for (i = 0U; i < 14U; ++i) {
        for (j = 0U; j < (CUBE_SAMPLE_COUNT - i - 1U); ++j) {
            if (hsv[index[j]].s < hsv[index[j + 1U]].s) {
                uint8_t tmp = index[j];
                index[j] = index[j + 1U];
                index[j + 1U] = tmp;
            }
        }
    }

    for (i = 0U; i < 39U; ++i) {
        for (j = 0U; j < (40U - i - 1U); ++j) {
            if (hsv[index[j]].h < hsv[index[j + 1U]].h) {
                uint8_t tmp = index[j];
                index[j] = index[j + 1U];
                index[j + 1U] = tmp;
            }
        }
    }
}

color_ret_t color_detect_cube(const color_rgb_t *samples, unsigned int sample_count, char *cube_str)
{
    hsv_internal_t hsv[CUBE_SAMPLE_COUNT];
    uint8_t index[CUBE_SAMPLE_COUNT];
    unsigned int i;
    unsigned int dark_count = 0U;
    color_hsv_t one;
    uint16_t h_shift;

    if (samples == 0 || cube_str == 0) {
        return COLOR_RET_INVALID_ARG;
    }

    if (sample_count != CUBE_SAMPLE_COUNT) {
        return COLOR_RET_INVALID_ARG;
    }

    for (i = 0U; i < CUBE_SAMPLE_COUNT; ++i) {
        one = color_rgb_to_hsv(samples[i]);
        h_shift = (uint16_t)((one.h + 360U - 281U) % 360U);

        hsv[i].h = h_shift;
        hsv[i].s = one.s;
        hsv[i].v = one.v;
        index[i] = (uint8_t)i;

        if (one.v < 30U) {
            ++dark_count;
        }
    }

    if (dark_count > 10U) {
        cube_str[0] = '\0';
        return COLOR_RET_NO_CUBE;
    }

    sort_s_desc_then_h_desc(hsv, index);

    memset(cube_str, 0, CUBE_SAMPLE_COUNT + 1U);
    for (i = 0U; i < 8U; ++i) {
        cube_str[index[i]] = 'F';
    }
    for (i = 8U; i < 16U; ++i) {
        cube_str[index[i]] = 'B';
    }
    for (i = 16U; i < 24U; ++i) {
        cube_str[index[i]] = 'U';
    }
    for (i = 24U; i < 32U; ++i) {
        cube_str[index[i]] = 'L';
    }
    for (i = 32U; i < 40U; ++i) {
        cube_str[index[i]] = 'R';
    }
    for (i = 40U; i < 48U; ++i) {
        cube_str[index[i]] = 'D';
    }

    cube_str[4] = 'U';
    cube_str[13] = 'R';
    cube_str[22] = 'F';
    cube_str[31] = 'D';
    cube_str[40] = 'L';
    cube_str[49] = 'B';
    cube_str[54] = '\0';

    return COLOR_RET_OK;
}
