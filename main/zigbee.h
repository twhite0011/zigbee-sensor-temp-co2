#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_zigbee_core.h"
#include "sdkconfig.h"
#include "sensors.h"
#if CONFIG_APP_ENABLE_CO2_SENSOR
#include "zcl/esp_zigbee_zcl_carbon_dioxide_measurement.h"
#endif

#define ZIGBEE_MANUFACTURER_NAME    "DIY"
#if CONFIG_APP_ENABLE_CO2_SENSOR
#define ZIGBEE_MODEL_ID             "XIAO-SHTC3-CO2"
#else
#define ZIGBEE_MODEL_ID             "XIAO-SHTC3"
#endif

#define EP_SENSOR                   1

#define ATTR_TEMP_MEASURED_VALUE    ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID
#define ATTR_HUM_MEASURED_VALUE     ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID
#if CONFIG_APP_ENABLE_CO2_SENSOR
#define ATTR_CO2_MEASURED_VALUE     ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MEASURED_VALUE_ID
#endif

void zigbee_register_endpoints(void);
void zigbee_report_temp_humidity(const shtc3_data_t *data);
#if CONFIG_APP_ENABLE_CO2_SENSOR
void zigbee_report_temp_humidity_co2(const shtc3_data_t *data, uint16_t co2_ppm);
#endif
void zigbee_signal_handler(esp_zb_app_signal_t *signal_s);
bool zigbee_is_joined(void);
