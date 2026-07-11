#ifndef MISC_H_
#define MISC_H_

#include <stdint.h>

typedef struct {
    uint8_t isPressing;
    uint8_t isDebouncedPressing;
    uint8_t isLongPressing;
    uint8_t pressEvent;
    uint8_t longPressEvent;
    uint32_t pressStartTick;
} BTN_stat_t;

void BTN_init(void);
void BtnTask(void);

#endif
