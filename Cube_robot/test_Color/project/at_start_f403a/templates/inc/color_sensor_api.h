#ifndef TPL_COLOR_SENSOR_API_H
#define TPL_COLOR_SENSOR_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COLOR_RET_OK = 0,
    COLOR_RET_IO_ERROR = -1,
    COLOR_RET_BAD_ID = -2,
    COLOR_RET_NOT_READY = -3,
    COLOR_RET_INVALID_ARG = -4,
    COLOR_RET_NO_CUBE = -5
} color_ret_t;

typedef struct {
    uint16_t r;
    uint16_t g;
    uint16_t b;
} color_rgb_t;

typedef struct {
    uint16_t h;
    uint16_t s;
    uint16_t v;
} color_hsv_t;

typedef enum {
    FACE_U = 0,
    FACE_R,
    FACE_F,
    FACE_D,
    FACE_L,
    FACE_B,
    FACE_UNKNOWN
} cube_face_t;

color_ret_t color_sensor_init(void);
color_ret_t color_sensor_read_rgb(color_rgb_t *out_rgb);
color_ret_t color_sensor_read_rgb_by_id(unsigned int sensor_id, color_rgb_t *out_rgb);
color_hsv_t color_rgb_to_hsv(color_rgb_t rgb);
cube_face_t get_color(color_rgb_t rgb);
color_ret_t color_detect_cube(const color_rgb_t *samples, unsigned int sample_count, char *cube_str);

#ifdef __cplusplus
}
#endif

#endif
