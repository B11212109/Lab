#ifndef COLOR_DETECT_H
#define COLOR_DETECT_H

#include <stdint.h>

void rgb2hsv(int r, int g, int b, int *h, int *s, int *v);
void color_detect(uint16_t *data, char *cube_str);

#endif
