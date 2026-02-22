#include "sensors.h"

#include <stddef.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sensors";

static i2c_master_bus_handle_t s_i2c_bus = NULL;
static i2c_master_dev_handle_t s_shtc3_dev = NULL;
static i2c_master_dev_handle_t s_scd4x_dev = NULL;

#define SHTC3_CMD_WAKEUP            0x3517
#define SHTC3_CMD_SLEEP             0xB098
#define SHTC3_CMD_MEAS_T_FIRST_NM   0x7866

#define SCD4X_CMD_START_PERIODIC    0x21B1
#define SCD4X_CMD_STOP_PERIODIC     0x3F86
#define SCD4X_CMD_REINIT            0x3646
#define SCD4X_CMD_DATA_READY        0xE4B8
#define SCD4X_CMD_READ_MEASUREMENT  0xEC05

static uint8_t crc8_poly31(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static esp_err_t i2c_send_cmd(i2c_master_dev_handle_t dev, uint16_t cmd)
{
    uint8_t buf[2] = {(uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF)};
    return i2c_master_transmit(dev, buf, sizeof(buf), 1000);
}

static void delay_ms_ceil(uint32_t ms)
{
    TickType_t ticks = (ms + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS;
    vTaskDelay(ticks > 0 ? ticks : 1);
}

static esp_err_t shtc3_wakeup(void)
{
    esp_err_t ret = i2c_send_cmd(s_shtc3_dev, SHTC3_CMD_WAKEUP);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SHTC3 wakeup cmd failed: %s", esp_err_to_name(ret));
    }
    delay_ms_ceil(2);
    return ret;
}

static esp_err_t shtc3_sleep(void)
{
    esp_err_t ret = i2c_send_cmd(s_shtc3_dev, SHTC3_CMD_SLEEP);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SHTC3 sleep cmd failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t setup_i2c_once(void)
{
    esp_err_t ret;

    if (s_i2c_bus == NULL) {
        i2c_master_bus_config_t bus_cfg = {
            .i2c_port = I2C_MASTER_PORT,
            .sda_io_num = I2C_SDA_PIN,
            .scl_io_num = I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };
        ret = i2c_new_master_bus(&bus_cfg, &s_i2c_bus);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    ret = i2c_master_probe(s_i2c_bus, SHTC3_I2C_ADDR, 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SHTC3 probe at 0x%02X failed: %s", SHTC3_I2C_ADDR, esp_err_to_name(ret));
        return ret;
    }

    if (s_shtc3_dev == NULL) {
        i2c_device_config_t shtc3_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = SHTC3_I2C_ADDR,
            .scl_speed_hz = I2C_MASTER_FREQ_HZ,
            .scl_wait_us = 20000,
        };
        ret = i2c_master_bus_add_device(s_i2c_bus, &shtc3_cfg, &s_shtc3_dev);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SHTC3 add device failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    ret = i2c_master_probe(s_i2c_bus, SCD4X_I2C_ADDR, 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SCD4X probe at 0x%02X failed: %s", SCD4X_I2C_ADDR, esp_err_to_name(ret));
        return ret;
    }

    if (s_scd4x_dev == NULL) {
        i2c_device_config_t scd4x_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = SCD4X_I2C_ADDR,
            .scl_speed_hz = I2C_MASTER_FREQ_HZ,
            .scl_wait_us = 20000,
        };
        ret = i2c_master_bus_add_device(s_i2c_bus, &scd4x_cfg, &s_scd4x_dev);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SCD4X add device failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    return ESP_OK;
}

esp_err_t sensors_init(void)
{
    esp_err_t ret = setup_i2c_once();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = shtc3_wakeup();
    if (ret == ESP_OK) {
        (void)shtc3_sleep();
    } else {
        return ret;
    }

    // SCD4X can take up to 1s after power-up before command acceptance.
    delay_ms_ceil(1000);

    ret = i2c_send_cmd(s_scd4x_dev, SCD4X_CMD_STOP_PERIODIC);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SCD4X stop periodic returned: %s", esp_err_to_name(ret));
    }
    delay_ms_ceil(500);

    ret = i2c_send_cmd(s_scd4x_dev, SCD4X_CMD_REINIT);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SCD4X reinit returned: %s", esp_err_to_name(ret));
    }
    delay_ms_ceil(30);

    ret = i2c_send_cmd(s_scd4x_dev, SCD4X_CMD_START_PERIODIC);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SCD4X start periodic failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wait for first periodic sample to become available.
    delay_ms_ceil(5500);

    ESP_LOGI(TAG, "SHTC3 initialized on I2C 0x%02X", SHTC3_I2C_ADDR);
    ESP_LOGI(TAG, "SCD4X initialized on I2C 0x%02X", SCD4X_I2C_ADDR);
    return ESP_OK;
}

esp_err_t sensors_read_shtc3(shtc3_data_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    bool awake = false;
    esp_err_t ret = shtc3_wakeup();
    if (ret != ESP_OK) {
        return ret;
    }
    awake = true;

    ret = i2c_send_cmd(s_shtc3_dev, SHTC3_CMD_MEAS_T_FIRST_NM);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SHTC3 measure cmd failed: %s", esp_err_to_name(ret));
        goto out;
    }

    delay_ms_ceil(13);

    uint8_t raw[6] = {0};
    ret = i2c_master_receive(s_shtc3_dev, raw, sizeof(raw), 1000);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SHTC3 payload read failed: %s", esp_err_to_name(ret));
        goto out;
    }

    if (crc8_poly31(&raw[0], 2) != raw[2] || crc8_poly31(&raw[3], 2) != raw[5]) {
        ESP_LOGW(TAG, "SHTC3 CRC check failed");
        ret = ESP_ERR_INVALID_CRC;
        goto out;
    }

    uint16_t t_raw = ((uint16_t)raw[0] << 8) | raw[1];
    uint16_t h_raw = ((uint16_t)raw[3] << 8) | raw[4];
    out->temperature_c = -45.0f + 175.0f * ((float)t_raw / 65535.0f);
    out->humidity_pct = 100.0f * ((float)h_raw / 65535.0f);
    ret = ESP_OK;

out:
    if (awake) {
        (void)shtc3_sleep();
    }
    return ret;
}

esp_err_t sensors_read_scd4x_co2(uint16_t *co2_ppm)
{
    if (co2_ppm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2c_send_cmd(s_scd4x_dev, SCD4X_CMD_DATA_READY);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SCD4X data-ready cmd failed: %s", esp_err_to_name(ret));
        return ret;
    }
    delay_ms_ceil(1);

    uint8_t status_raw[3] = {0};
    ret = i2c_master_receive(s_scd4x_dev, status_raw, sizeof(status_raw), 1000);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SCD4X data-ready read failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (crc8_poly31(&status_raw[0], 2) != status_raw[2]) {
        ESP_LOGW(TAG, "SCD4X data-ready CRC check failed");
        return ESP_ERR_INVALID_CRC;
    }

    uint16_t status = ((uint16_t)status_raw[0] << 8) | status_raw[1];
    if ((status & 0x07FFu) == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    ret = i2c_send_cmd(s_scd4x_dev, SCD4X_CMD_READ_MEASUREMENT);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SCD4X read-measurement cmd failed: %s", esp_err_to_name(ret));
        return ret;
    }
    delay_ms_ceil(2);

    uint8_t raw[9] = {0};
    ret = i2c_master_receive(s_scd4x_dev, raw, sizeof(raw), 1000);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SCD4X measurement read failed: %s", esp_err_to_name(ret));
        return ret;
    }

    for (int i = 0; i < 3; i++) {
        if (crc8_poly31(&raw[i * 3], 2) != raw[i * 3 + 2]) {
            ESP_LOGW(TAG, "SCD4X measurement CRC check failed at block %d", i);
            return ESP_ERR_INVALID_CRC;
        }
    }

    *co2_ppm = ((uint16_t)raw[0] << 8) | raw[1];
    return ESP_OK;
}
