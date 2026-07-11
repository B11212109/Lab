#ifndef BSP_HMI_H
#define BSP_HMI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ACTION_NONE = 0,
    ACTION_UP,
    ACTION_DOWN,
    ACTION_LEFT,
    ACTION_RIGHT,
    ACTION_CLICK
} hmi_action_t;

void bsp_hmi_init(void);
uint16_t hmi_get_joy_x(void);
uint16_t hmi_get_joy_y(void);
uint16_t hmi_get_speed_adc(void);
uint8_t hmi_get_speed_percent(void);
void hmi_set_speed_percent(uint8_t percent);
void hmi_reset_encoder_delta(void);
int16_t hmi_consume_encoder_steps(void);
uint8_t hmi_consume_reset_button(void);
hmi_action_t hmi_get_action(void);
hmi_action_t hmi_get_last_action(void);

#ifdef __cplusplus
}
#endif

#endif
