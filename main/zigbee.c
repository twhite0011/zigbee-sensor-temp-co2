#include "zigbee.h"

#include <stdatomic.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "zcl/esp_zigbee_zcl_command.h"
#include "zdo/esp_zigbee_zdo_command.h"

static const char *TAG = "zigbee";

static char s_manufacturer_name[] = "\x03" "DIY";
static char s_model_identifier[]  = "\x0E" "XIAO-SHTC3-CO2";

#define ZIGBEE_TX_POWER_DBM            (+3)
#define STEERING_RETRY_DELAY_MS        (30000)
#define COORDINATOR_SHORT_ADDR         0x0000
#define DEFAULT_COORDINATOR_ENDPOINT   1

static atomic_bool s_joined = ATOMIC_VAR_INIT(false);
static atomic_uchar s_coordinator_endpoint = ATOMIC_VAR_INIT(DEFAULT_COORDINATOR_ENDPOINT);

static uint8_t coordinator_endpoint_get(void)
{
    uint8_t endpoint = atomic_load_explicit(&s_coordinator_endpoint, memory_order_relaxed);
    if (endpoint == 0) {
        endpoint = DEFAULT_COORDINATOR_ENDPOINT;
    }
    return endpoint;
}

static void coordinator_endpoint_set(uint8_t endpoint)
{
    if (endpoint != 0) {
        atomic_store_explicit(&s_coordinator_endpoint, endpoint, memory_order_relaxed);
    }
}

static void coordinator_active_ep_cb(esp_zb_zdp_status_t zdo_status,
                                     uint8_t ep_count,
                                     uint8_t *ep_id_list,
                                     void *user_ctx)
{
    (void)user_ctx;

    if (zdo_status != ESP_ZB_ZDP_STATUS_SUCCESS || ep_count == 0 || ep_id_list == NULL) {
        ESP_LOGW(TAG, "Active EP discovery failed status=%d count=%u, keeping endpoint %u",
                 zdo_status, (unsigned)ep_count, (unsigned)coordinator_endpoint_get());
        return;
    }

    uint8_t selected_ep = 0;
    for (uint8_t i = 0; i < ep_count; i++) {
        if (ep_id_list[i] != 0) {
            selected_ep = ep_id_list[i];
            break;
        }
    }

    if (selected_ep == 0) {
        ESP_LOGW(TAG, "Active EP response had only invalid endpoints, keeping endpoint %u",
                 (unsigned)coordinator_endpoint_get());
        return;
    }

    coordinator_endpoint_set(selected_ep);
    ESP_LOGI(TAG, "Coordinator endpoint discovered: %u", (unsigned)selected_ep);
}

static void discover_coordinator_endpoint(void)
{
    esp_zb_zdo_active_ep_req_param_t req = {
        .addr_of_interest = COORDINATOR_SHORT_ADDR,
    };

    esp_zb_zdo_active_ep_req(&req, coordinator_active_ep_cb, NULL);
}

static esp_err_t send_one_shot_report(uint16_t cluster_id, uint16_t attribute_id)
{
    esp_zb_zcl_report_attr_cmd_t cmd_req = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = COORDINATOR_SHORT_ADDR,
            .dst_endpoint = coordinator_endpoint_get(),
            .src_endpoint = EP_SENSOR,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID = cluster_id,
        .manuf_specific = 0,
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI,
        .dis_default_resp = 1,
        .manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC,
        .attributeID = attribute_id,
    };

    return esp_zb_zcl_report_attr_cmd_req(&cmd_req);
}

static void commissioning_retry_cb(uint8_t mode_mask)
{
    esp_err_t ret = esp_zb_bdb_start_top_level_commissioning(mode_mask);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Commissioning start failed mode=0x%02X err=%s",
                 mode_mask, esp_err_to_name(ret));
    }
}

bool zigbee_is_joined(void)
{
    return atomic_load_explicit(&s_joined, memory_order_relaxed);
}

static int16_t celsius_to_zcl(float c)
{
    return (int16_t)(c * 100.0f);
}

static uint16_t pct_to_zcl(float pct)
{
    return (uint16_t)(pct * 100.0f);
}

static float co2_ppm_to_zcl_fraction(uint16_t ppm)
{
    return (float)ppm / 1000000.0f;
}

void zigbee_register_endpoints(void)
{
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();

    esp_zb_basic_cluster_cfg_t basic_cfg = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = 0x03,
    };
    esp_zb_attribute_list_t *basic_attr = esp_zb_basic_cluster_create(&basic_cfg);
    esp_zb_basic_cluster_add_attr(basic_attr, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, s_manufacturer_name);
    esp_zb_basic_cluster_add_attr(basic_attr, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, s_model_identifier);

    esp_zb_identify_cluster_cfg_t identify_cfg = {.identify_time = 0};
    esp_zb_attribute_list_t *identify_attr = esp_zb_identify_cluster_create(&identify_cfg);

    int16_t temp_default = 0;
    int16_t temp_min = -4000;
    int16_t temp_max = 8500;
    esp_zb_temperature_meas_cluster_cfg_t temp_cfg = {
        .measured_value = temp_default,
        .min_value = temp_min,
        .max_value = temp_max,
    };
    esp_zb_attribute_list_t *temp_attr = esp_zb_temperature_meas_cluster_create(&temp_cfg);

    uint16_t hum_default = 0;
    uint16_t hum_min = 0;
    uint16_t hum_max = 10000;
    esp_zb_humidity_meas_cluster_cfg_t hum_cfg = {
        .measured_value = hum_default,
        .min_value = hum_min,
        .max_value = hum_max,
    };
    esp_zb_attribute_list_t *hum_attr = esp_zb_humidity_meas_cluster_create(&hum_cfg);

    esp_zb_carbon_dioxide_measurement_cluster_cfg_t co2_cfg = {
        .measured_value = ESP_ZB_ZCL_CARBON_DIOXIDE_MEASUREMENT_MEASURED_VALUE_DEFAULT,
        .min_measured_value = 0.0004f,
        .max_measured_value = 0.0050f,
    };
    esp_zb_attribute_list_t *co2_attr = esp_zb_carbon_dioxide_measurement_cluster_create(&co2_cfg);

    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_attr, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(cluster_list, identify_attr, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_temperature_meas_cluster(cluster_list, temp_attr, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_humidity_meas_cluster(cluster_list, hum_attr, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_carbon_dioxide_measurement_cluster(cluster_list, co2_attr, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    esp_zb_endpoint_config_t ep_cfg = {
        .endpoint = EP_SENSOR,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID,
        .app_device_version = 0,
    };
    esp_zb_ep_list_add_ep(ep_list, cluster_list, ep_cfg);

    esp_zb_device_register(ep_list);
    ESP_LOGI(TAG, "Sensor endpoint registered");
}

void zigbee_report_sensor_data(const shtc3_data_t *data, uint16_t co2_ppm)
{
    int16_t temp_val = celsius_to_zcl(data->temperature_c);
    uint16_t hum_val = pct_to_zcl(data->humidity_pct);
    float co2_val = co2_ppm_to_zcl_fraction(co2_ppm);

    esp_err_t first_err = ESP_OK;
    esp_err_t ret;

    esp_zb_lock_acquire(portMAX_DELAY);

    ret = esp_zb_zcl_set_attribute_val(
        EP_SENSOR,
        ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ATTR_TEMP_MEASURED_VALUE,
        &temp_val,
        false);
    if (ret != ESP_OK && first_err == ESP_OK) {
        first_err = ret;
    }

    ret = esp_zb_zcl_set_attribute_val(
        EP_SENSOR,
        ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ATTR_HUM_MEASURED_VALUE,
        &hum_val,
        false);
    if (ret != ESP_OK && first_err == ESP_OK) {
        first_err = ret;
    }

    ret = esp_zb_zcl_set_attribute_val(
        EP_SENSOR,
        ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ATTR_CO2_MEASURED_VALUE,
        &co2_val,
        false);
    if (ret != ESP_OK && first_err == ESP_OK) {
        first_err = ret;
    }

    ret = send_one_shot_report(ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ATTR_TEMP_MEASURED_VALUE);
    if (ret != ESP_OK && first_err == ESP_OK) {
        first_err = ret;
    }

    ret = send_one_shot_report(ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, ATTR_HUM_MEASURED_VALUE);
    if (ret != ESP_OK && first_err == ESP_OK) {
        first_err = ret;
    }

    ret = send_one_shot_report(ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT, ATTR_CO2_MEASURED_VALUE);
    if (ret != ESP_OK && first_err == ESP_OK) {
        first_err = ret;
    }

    esp_zb_lock_release();

    if (first_err == ESP_OK) {
        ESP_LOGI(TAG, "Reported temp=%.2f C humidity=%.2f %% co2=%u ppm",
                 data->temperature_c, data->humidity_pct, (unsigned)co2_ppm);
    } else {
        ESP_LOGW(TAG, "Failed to update one or more ZCL attributes: %s",
                 esp_err_to_name(first_err));
    }
}

void zigbee_signal_handler(esp_zb_app_signal_t *signal_s)
{
    uint32_t *p_sg_p = signal_s->p_app_signal;
    esp_err_t err = signal_s->esp_err_status;
    esp_zb_app_signal_type_t sig = *p_sg_p;

    switch (sig) {
        case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
            ESP_LOGI(TAG, "Zigbee stack initialized");
            esp_zb_set_tx_power(ZIGBEE_TX_POWER_DBM);
            ESP_LOGI(TAG, "Zigbee TX power set to %d dBm", ZIGBEE_TX_POWER_DBM);
            commissioning_retry_cb(ESP_ZB_BDB_MODE_INITIALIZATION);
            break;

        case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
            if (err == ESP_OK) {
                bool is_factory_new = esp_zb_bdb_is_factory_new();
                ESP_LOGI(TAG, "Device start mode: %s", is_factory_new ? "factory_new" : "reboot");
                if (is_factory_new) {
                    ESP_LOGI(TAG, "Start network steering");
                    commissioning_retry_cb(ESP_ZB_BDB_MODE_NETWORK_STEERING);
                } else {
                    atomic_store_explicit(&s_joined, true, memory_order_relaxed);
                    ESP_LOGI(TAG, "Rejoined network PAN=0x%04X CH=%d SHORT=0x%04X",
                             esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
                    discover_coordinator_endpoint();
                }
            } else {
                ESP_LOGW(TAG, "BDB start/reboot status error: %s", esp_err_to_name(err));
                esp_zb_scheduler_alarm(commissioning_retry_cb, ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
            }
            break;

        case ESP_ZB_BDB_SIGNAL_STEERING:
            if (err == ESP_OK) {
                atomic_store_explicit(&s_joined, true, memory_order_relaxed);
                ESP_LOGI(TAG, "Joined network PAN=0x%04X CH=%d SHORT=0x%04X",
                         esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
                discover_coordinator_endpoint();
            } else {
                atomic_store_explicit(&s_joined, false, memory_order_relaxed);
                coordinator_endpoint_set(DEFAULT_COORDINATOR_ENDPOINT);
                ESP_LOGW(TAG, "Steering failed (%s), retrying in %d s",
                         esp_err_to_name(err), STEERING_RETRY_DELAY_MS / 1000);
                esp_zb_scheduler_alarm(commissioning_retry_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING,
                                       STEERING_RETRY_DELAY_MS);
            }
            break;

        case ESP_ZB_ZDO_SIGNAL_LEAVE:
            atomic_store_explicit(&s_joined, false, memory_order_relaxed);
            coordinator_endpoint_set(DEFAULT_COORDINATOR_ENDPOINT);
            ESP_LOGW(TAG, "Left network");
            break;

        default:
            ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s",
                     esp_zb_zdo_signal_to_string(sig), sig, esp_err_to_name(err));
            break;
    }
}
