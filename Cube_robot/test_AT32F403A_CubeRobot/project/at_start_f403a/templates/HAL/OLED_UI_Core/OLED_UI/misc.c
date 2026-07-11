#include "misc.h"

BTN_stat_t BTN_stat;

void BTN_init(void)
{
    BTN_stat.isPressing = 0U;
    BTN_stat.isDebouncedPressing = 0U;
    BTN_stat.isLongPressing = 0U;
    BTN_stat.pressEvent = 0U;
    BTN_stat.longPressEvent = 0U;
    BTN_stat.pressStartTick = 0U;
}

void BtnTask(void)
{
}
