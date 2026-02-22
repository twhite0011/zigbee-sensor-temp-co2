#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_zigbee_core.h"
#include "platform/esp_zigbee_platform.h"

#include "zigbee.h"
#include "sensors.h"

static const char *TAG = "main";

#define ZIGBEE_PRIMARY_CHANNEL_MASK   (1UL << 11)
#define ZIGBEE_SECONDARY_CHANNEL_MASK (1UL << 11)
#define REPORT_INTERVAL_MS            (30000)

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    zigbee_signal_handler(signal_struct);
}

static void zigbee_task(void *arg)
{
    esp_zb_cfg_t zb_nwk_cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,
        .install_code_policy = false,
        .nwk_cfg.zed_cfg = {
            .ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN,
            .keep_alive = 3000,
        },
    };

    esp_zb_sleep_enable(false);
    esp_zb_init(&zb_nwk_cfg);

    zigbee_register_endpoints();

    ESP_ERROR_CHECK(esp_zb_set_primary_network_channel_set(ZIGBEE_PRIMARY_CHANNEL_MASK));
    ESP_ERROR_CHECK(esp_zb_set_secondary_network_channel_set(ZIGBEE_SECONDARY_CHANNEL_MASK));
    ESP_LOGI(TAG, "Zigbee channel masks: primary=0x%08lX secondary=0x%08lX",
             (unsigned long)ZIGBEE_PRIMARY_CHANNEL_MASK,
             (unsigned long)ZIGBEE_SECONDARY_CHANNEL_MASK);

    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}

static void sensor_task(void *arg)
{
    while (!zigbee_is_joined()) {
        ESP_LOGI(TAG, "Waiting for Zigbee join...");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    while (sensors_init() != ESP_OK) {
        ESP_LOGW(TAG, "Sensor init failed, retrying in 5 s");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    ESP_LOGI(TAG, "Sensor task started: reporting every %d s", REPORT_INTERVAL_MS / 1000);

    while (true) {
        if (zigbee_is_joined()) {
            shtc3_data_t data;
            uint16_t co2_ppm = 0;

            esp_err_t th_ret = sensors_read_shtc3(&data);
            esp_err_t co2_ret = sensors_read_scd4x_co2(&co2_ppm);

            if (th_ret == ESP_OK && co2_ret == ESP_OK) {
                zigbee_report_sensor_data(&data, co2_ppm);
            } else {
                if (th_ret != ESP_OK) {
                    ESP_LOGW(TAG, "SHTC3 read failed: %s", esp_err_to_name(th_ret));
                }
                if (co2_ret != ESP_OK) {
                    ESP_LOGW(TAG, "SCD4X CO2 read failed: %s", esp_err_to_name(co2_ret));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(REPORT_INTERVAL_MS));
    }
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set("main", ESP_LOG_INFO);
    esp_log_level_set("zigbee", ESP_LOG_INFO);
    esp_log_level_set("sensors", ESP_LOG_INFO);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_zb_platform_config_t config = {
        .radio_config = {
            .radio_mode = ZB_RADIO_MODE_NATIVE,
        },
        .host_config = {
            .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,
        },
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    ESP_LOGI(TAG, "Starting XIAO ESP32-C6 SHTC3 + SCD4X Zigbee sensor (30 s reporting)");

    xTaskCreate(zigbee_task, "zigbee", 8192, NULL, 5, NULL);
    xTaskCreate(sensor_task, "sensor", 8192, NULL, 4, NULL);
}
