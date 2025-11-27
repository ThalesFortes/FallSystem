#ifndef VL53L0X_H
#define VL53L0X_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/i2c.h"

// Endereço I2C padrão
#define VL53L0X_ADDR   0x29

// API mínima
bool vl53l0x_init(i2c_inst_t *i2c);
bool vl53l0x_start_continuous(i2c_inst_t *i2c);
bool vl53l0x_stop_continuous(i2c_inst_t *i2c);
bool vl53l0x_read_range_mm(i2c_inst_t *i2c, uint16_t *range_mm, uint32_t timeout_ms);

#endif