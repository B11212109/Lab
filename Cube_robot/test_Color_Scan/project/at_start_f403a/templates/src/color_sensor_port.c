#include "../inc/color_sensor_port.h"

#include "at32f403a_407_board.h"
#include "../inc/color_sensor_cfg.h"

#if defined(AT32F403Axx) || defined(AT32F407xx)

typedef struct {
    gpio_type *gpio_port;
    crm_periph_clock_type gpio_clk;
    uint16_t scl_pin;
    uint16_t sda_pin;
    uint8_t need_remap;
    uint32_t remap;
} color_i2c_bus_t;

static const color_i2c_bus_t g_color_bus[COLOR_SENSOR_COUNT] = {
    {
        COLOR0_I2C_GPIO_PORT,
        COLOR0_I2C_GPIO_CRM_CLK,
        COLOR0_I2C_SCL_PIN,
        COLOR0_I2C_SDA_PIN,
        COLOR0_I2C_NEED_REMAP,
        COLOR0_I2C_MUX_REMAP
    },
    {
        COLOR1_I2C_GPIO_PORT,
        COLOR1_I2C_GPIO_CRM_CLK,
        COLOR1_I2C_SCL_PIN,
        COLOR1_I2C_SDA_PIN,
        COLOR1_I2C_NEED_REMAP,
        0U
    }
};

volatile int g_color_i2c_last_sensor = -1;
volatile int g_color_i2c_last_step = 0;
volatile unsigned long g_color_i2c_last_flag = 0UL;

static void color_i2c_diag_set(int sensor_id, int step, unsigned long flag)
{
    g_color_i2c_last_sensor = sensor_id;
    g_color_i2c_last_step = step;
    g_color_i2c_last_flag = flag;
}

static void color_i2c_delay(void)
{
    delay_us(2U);
}

static void color_i2c_release_line(const color_i2c_bus_t *bus, uint16_t pin)
{
    gpio_init_type gpio_init_struct;

    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_pins = pin;
    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
    gpio_init_struct.gpio_pull = GPIO_PULL_UP;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_OPEN_DRAIN;
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init(bus->gpio_port, &gpio_init_struct);
}

static void color_i2c_drive_low(const color_i2c_bus_t *bus, uint16_t pin)
{
    gpio_init_type gpio_init_struct;

    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_pins = pin;
    gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_OPEN_DRAIN;
    gpio_init_struct.gpio_pull = GPIO_PULL_UP;
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init(bus->gpio_port, &gpio_init_struct);
    gpio_bits_reset(bus->gpio_port, pin);
}

static void color_i2c_sda_release(const color_i2c_bus_t *bus)
{
    color_i2c_release_line(bus, bus->sda_pin);
}

static void color_i2c_sda_low(const color_i2c_bus_t *bus)
{
    color_i2c_drive_low(bus, bus->sda_pin);
}

static void color_i2c_scl_release(const color_i2c_bus_t *bus)
{
    color_i2c_release_line(bus, bus->scl_pin);
}

static void color_i2c_scl_low(const color_i2c_bus_t *bus)
{
    color_i2c_drive_low(bus, bus->scl_pin);
}

static uint8_t color_i2c_sda_read(const color_i2c_bus_t *bus)
{
    return (uint8_t)gpio_input_data_bit_read(bus->gpio_port, bus->sda_pin);
}

static void color_i2c_start(const color_i2c_bus_t *bus)
{
    color_i2c_sda_release(bus);
    color_i2c_delay();
    color_i2c_scl_release(bus);
    color_i2c_delay();
    color_i2c_sda_low(bus);
    color_i2c_delay();
    color_i2c_scl_low(bus);
    color_i2c_delay();
}

static void color_i2c_stop(const color_i2c_bus_t *bus)
{
    color_i2c_sda_low(bus);
    color_i2c_delay();
    color_i2c_scl_release(bus);
    color_i2c_delay();
    color_i2c_sda_release(bus);
    color_i2c_delay();
}

static uint8_t color_i2c_send_byte(const color_i2c_bus_t *bus, uint8_t data)
{
    uint8_t i;

    for (i = 0U; i < 8U; ++i) {
        if ((data & 0x80U) != 0U) {
            color_i2c_sda_release(bus);
        } else {
            color_i2c_sda_low(bus);
        }

        color_i2c_delay();
        color_i2c_scl_release(bus);
        color_i2c_delay();
        color_i2c_scl_low(bus);
        color_i2c_delay();
        data <<= 1;
    }

    color_i2c_sda_release(bus);
    color_i2c_delay();
    color_i2c_scl_release(bus);
    color_i2c_delay();
    i = color_i2c_sda_read(bus);
    color_i2c_scl_low(bus);
    color_i2c_delay();
    return i;
}

static uint8_t color_i2c_get_byte(const color_i2c_bus_t *bus, uint8_t send_ack)
{
    uint8_t i;
    uint8_t data = 0U;

    color_i2c_sda_release(bus);
    color_i2c_delay();
    for (i = 0U; i < 8U; ++i) {
        data <<= 1;
        color_i2c_scl_release(bus);
        color_i2c_delay();
        data |= color_i2c_sda_read(bus);
        color_i2c_scl_low(bus);
        color_i2c_delay();
    }

    if (send_ack != 0U) {
        color_i2c_sda_low(bus);
    } else {
        color_i2c_sda_release(bus);
    }
    color_i2c_delay();
    color_i2c_scl_release(bus);
    color_i2c_delay();
    color_i2c_scl_low(bus);
    color_i2c_delay();
    color_i2c_sda_release(bus);
    return data;
}

static uint8_t color_i2c_addr_make(uint8_t dev7, uint8_t read)
{
    uint8_t base;

    if (dev7 < 0x80U) {
        base = (uint8_t)(dev7 << 1);
    } else {
        base = (uint8_t)(dev7 & 0xFEU);
    }
    return (uint8_t)(base | (read & 0x01U));
}

static color_ret_t color_bus_get(unsigned int sensor_id, const color_i2c_bus_t **out)
{
    if (out == 0) {
        return COLOR_RET_INVALID_ARG;
    }
    if (sensor_id >= COLOR_SENSOR_COUNT) {
        return COLOR_RET_INVALID_ARG;
    }
    *out = &g_color_bus[sensor_id];
    return COLOR_RET_OK;
}

color_ret_t color_port_i2c_init_all(void)
{
    unsigned int i;
    const color_i2c_bus_t *bus;

    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);

    for (i = 0U; i < COLOR_SENSOR_COUNT; ++i) {
        bus = &g_color_bus[i];

        crm_periph_clock_enable(bus->gpio_clk, TRUE);

        if (bus->need_remap != 0U) {
            gpio_pin_remap_config(bus->remap, TRUE);
        }

        color_i2c_sda_release(bus);
        color_i2c_scl_release(bus);
    }

    color_i2c_diag_set(-1, 0, 0U);

    return COLOR_RET_OK;
}

color_ret_t color_port_i2c_write_reg(unsigned int sensor_id, uint8_t dev7, uint8_t reg, uint8_t value)
{
    uint8_t out[2];
    out[0] = reg;
    out[1] = value;
    return color_port_i2c_write(sensor_id, dev7, out, 2U);
}

color_ret_t color_port_i2c_read_regs(unsigned int sensor_id, uint8_t dev7, uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;
    color_ret_t ret;
    const color_i2c_bus_t *bus;

    if (buf == 0 || len == 0U) {
        return COLOR_RET_INVALID_ARG;
    }

    ret = color_bus_get(sensor_id, &bus);
    if (ret != COLOR_RET_OK) {
        return ret;
    }

    color_i2c_start(bus);
    if (color_i2c_send_byte(bus, color_i2c_addr_make(dev7, 0U)) != 0U) {
        color_i2c_diag_set((int)sensor_id, 20, 0UL);
        color_i2c_stop(bus);
        return COLOR_RET_IO_ERROR;
    }
    if (color_i2c_send_byte(bus, reg) != 0U) {
        color_i2c_diag_set((int)sensor_id, 21, (unsigned long)reg);
        color_i2c_stop(bus);
        return COLOR_RET_IO_ERROR;
    }

    color_i2c_start(bus);
    if (color_i2c_send_byte(bus, color_i2c_addr_make(dev7, 1U)) != 0U) {
        color_i2c_diag_set((int)sensor_id, 22, 0UL);
        color_i2c_stop(bus);
        return COLOR_RET_IO_ERROR;
    }

    for (i = 0U; i < len; ++i) {
        buf[i] = color_i2c_get_byte(bus, (i + 1U) < len ? 1U : 0U);
    }
    color_i2c_stop(bus);
    return COLOR_RET_OK;
}

color_ret_t color_port_i2c_write(unsigned int sensor_id, uint8_t dev7, const uint8_t *buf, uint8_t len)
{
    uint8_t i;
    color_ret_t ret;
    const color_i2c_bus_t *bus;

    if (buf == 0 || len == 0U) {
        return COLOR_RET_INVALID_ARG;
    }

    ret = color_bus_get(sensor_id, &bus);
    if (ret != COLOR_RET_OK) {
        return ret;
    }

    color_i2c_start(bus);
    if (color_i2c_send_byte(bus, color_i2c_addr_make(dev7, 0U)) != 0U) {
        color_i2c_diag_set((int)sensor_id, 30, 0UL);
        color_i2c_stop(bus);
        return COLOR_RET_IO_ERROR;
    }

    for (i = 0U; i < len; ++i) {
        if (color_i2c_send_byte(bus, buf[i]) != 0U) {
            color_i2c_diag_set((int)sensor_id, 31, (unsigned long)buf[i]);
            color_i2c_stop(bus);
            return COLOR_RET_IO_ERROR;
        }
    }

    color_i2c_stop(bus);
    return COLOR_RET_OK;
}

color_ret_t color_port_i2c_read(unsigned int sensor_id, uint8_t dev7, uint8_t *buf, uint8_t len)
{
    uint8_t i;
    color_ret_t ret;
    const color_i2c_bus_t *bus;

    if (buf == 0 || len == 0U) {
        return COLOR_RET_INVALID_ARG;
    }

    ret = color_bus_get(sensor_id, &bus);
    if (ret != COLOR_RET_OK) {
        return ret;
    }

    color_i2c_start(bus);
    if (color_i2c_send_byte(bus, color_i2c_addr_make(dev7, 1U)) != 0U) {
        color_i2c_diag_set((int)sensor_id, 40, 0UL);
        color_i2c_stop(bus);
        return COLOR_RET_IO_ERROR;
    }

    for (i = 0U; i < len; ++i) {
        buf[i] = color_i2c_get_byte(bus, (i + 1U) < len ? 1U : 0U);
    }

    color_i2c_stop(bus);
    return COLOR_RET_OK;
}

void color_port_delay_ms(unsigned int ms)
{
    delay_ms((uint16_t)ms);
}

#else

color_ret_t color_port_i2c_init_all(void)
{
    return COLOR_RET_NOT_READY;
}

color_ret_t color_port_i2c_write_reg(unsigned int sensor_id, uint8_t dev7, uint8_t reg, uint8_t value)
{
    (void)sensor_id;
    (void)dev7;
    (void)reg;
    (void)value;
    return COLOR_RET_NOT_READY;
}

color_ret_t color_port_i2c_read_regs(unsigned int sensor_id, uint8_t dev7, uint8_t reg, uint8_t *buf, uint8_t len)
{
    (void)sensor_id;
    (void)dev7;
    (void)reg;
    (void)buf;
    (void)len;
    return COLOR_RET_NOT_READY;
}

color_ret_t color_port_i2c_write(unsigned int sensor_id, uint8_t dev7, const uint8_t *buf, uint8_t len)
{
    (void)sensor_id;
    (void)dev7;
    (void)buf;
    (void)len;
    return COLOR_RET_NOT_READY;
}

color_ret_t color_port_i2c_read(unsigned int sensor_id, uint8_t dev7, uint8_t *buf, uint8_t len)
{
    (void)sensor_id;
    (void)dev7;
    (void)buf;
    (void)len;
    return COLOR_RET_NOT_READY;
}

void color_port_delay_ms(unsigned int ms)
{
    (void)ms;
}

#endif
