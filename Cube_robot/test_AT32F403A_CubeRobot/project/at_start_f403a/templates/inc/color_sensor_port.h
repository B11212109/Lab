#ifndef TPL_COLOR_SENSOR_PORT_H
#define TPL_COLOR_SENSOR_PORT_H

#include <stdint.h>
#include "color_sensor_api.h"

#ifdef __cplusplus
extern "C" {
#endif

color_ret_t color_port_i2c_init_all(void);
color_ret_t color_port_i2c_write_reg(unsigned int sensor_id, uint8_t dev7, uint8_t reg, uint8_t value);
color_ret_t color_port_i2c_read_regs(unsigned int sensor_id, uint8_t dev7, uint8_t reg, uint8_t *buf, uint8_t len);
color_ret_t color_port_i2c_write(unsigned int sensor_id, uint8_t dev7, const uint8_t *buf, uint8_t len);
color_ret_t color_port_i2c_read(unsigned int sensor_id, uint8_t dev7, uint8_t *buf, uint8_t len);
void color_port_delay_ms(unsigned int ms);

#ifdef __cplusplus
}
#endif

#endif
