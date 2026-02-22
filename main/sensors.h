#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#define I2C_MASTER_PORT     I2C_NUM_0
#define I2C_SDA_PIN         22
#define I2C_SCL_PIN         23
#define I2C_MASTER_FREQ_HZ  100000

#define SHTC3_I2C_ADDR      0x70
#define SCD4X_I2C_ADDR      0x62

typedef struct {
    float temperature_c;
    float humidity_pct;
} shtc3_data_t;

esp_err_t sensors_init(void);
esp_err_t sensors_read_shtc3(shtc3_data_t *out);
esp_err_t sensors_read_scd4x_co2(uint16_t *co2_ppm);
