#ifndef TPL_COLOR_SENSOR_CFG_H
#define TPL_COLOR_SENSOR_CFG_H

#include "at32f403a_407.h"

#define COLOR_SENSOR_COUNT              2U

/* Sensor 0: I2C1 wiring, bit-banged on PB6/PB7 */
#define COLOR0_I2C_PERIPH               I2C1
#define COLOR0_I2C_CRM_CLK              CRM_I2C1_PERIPH_CLOCK
#define COLOR0_I2C_GPIO_PORT            GPIOB
#define COLOR0_I2C_GPIO_CRM_CLK         CRM_GPIOB_PERIPH_CLOCK
#define COLOR0_I2C_SCL_PIN              GPIO_PINS_6
#define COLOR0_I2C_SDA_PIN              GPIO_PINS_7
#define COLOR0_I2C_NEED_REMAP           0U
#define COLOR0_I2C_MUX_REMAP            I2C1_MUX

/* Sensor 1: I2C2 on PB10/PB11 (default route) */
#define COLOR1_I2C_PERIPH               I2C2
#define COLOR1_I2C_CRM_CLK              CRM_I2C2_PERIPH_CLOCK
#define COLOR1_I2C_GPIO_PORT            GPIOB
#define COLOR1_I2C_GPIO_CRM_CLK         CRM_GPIOB_PERIPH_CLOCK
#define COLOR1_I2C_SCL_PIN              GPIO_PINS_10
#define COLOR1_I2C_SDA_PIN              GPIO_PINS_11
#define COLOR1_I2C_NEED_REMAP           0U

#define COLOR_I2C_SPEED_HZ              100000U
#define TCS3472_I2C_ADDR_7BIT           0x29U
#define COLOR_I2C_TIMEOUT               1000000U

#define TCS3472_ATIME_DEFAULT           0xFFU
#define TCS3472_WTIME_DEFAULT           0xFFU
#define TCS3472_CONTROL_DEFAULT         0x01U

#define TCS3472_ID_EXPECTED             0x4DU

#define TCS3472_CMD_BIT                 0x80U
#define TCS3472_CMD_AUTO_INC            0x20U

#define TCS3472_REG_ENABLE              0x00U
#define TCS3472_REG_ATIME               0x01U
#define TCS3472_REG_WTIME               0x03U
#define TCS3472_REG_ID                  0x12U
#define TCS3472_REG_CDATAL              0x14U
#define TCS3472_REG_RDATAL              0x16U
#define TCS3472_REG_CONTROL             0x0FU

#define TCS3472_ENABLE_PON              0x01U
#define TCS3472_ENABLE_AEN              0x02U

#endif
