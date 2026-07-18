#include <assert.h>
#include <string.h>

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "gatt_server.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "license_verify.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"

#define LITEJAM_SERVICE_UUID 0xfff0

static const char *TAG = "litejam_main";
static uint8_t address_type;

static void advertise(void);

static int gap_event(struct ble_gap_event *event, void *argument)
{
    (void)argument;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "BLE connection status=%d", event->connect.status);
        if (event->connect.status != 0) {
            advertise();
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE disconnected reason=%d", event->disconnect.reason);
        gatt_server_on_disconnect();
        advertise();
        return 0;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        advertise();
        return 0;
    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "BLE MTU=%d", event->mtu.value);
        return 0;
    default:
        return 0;
    }
}

static void advertise(void)
{
    static const ble_uuid16_t service_uuid = BLE_UUID16_INIT(LITEJAM_SERVICE_UUID);
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)ble_svc_gap_device_name();
    fields.name_len = strlen((char *)fields.name);
    fields.name_is_complete = 1;
    fields.uuids16 = (ble_uuid16_t *)&service_uuid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;
    int result = ble_gap_adv_set_fields(&fields);
    if (result != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields failed: %d", result);
        return;
    }

    struct ble_gap_adv_params parameters = {0};
    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;
    result = ble_gap_adv_start(
        address_type,
        NULL,
        BLE_HS_FOREVER,
        &parameters,
        gap_event,
        NULL
    );
    if (result != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed: %d", result);
    }
}

static void on_sync(void)
{
    int result = ble_hs_id_infer_auto(0, &address_type);
    assert(result == 0);
    advertise();
}

static void host_task(void *parameter)
{
    (void)parameter;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void confirm_or_report_rollback(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY) {
        if (license_verify_public_key_ready()) {
            ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
            ESP_LOGI(TAG, "OTA image marked valid after startup self-check");
        } else {
            ESP_LOGE(TAG, "License public key self-check failed; rollback remains pending");
        }
    }
}

void app_main(void)
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);

    if (!license_verify_public_key_ready()) {
        ESP_LOGE(TAG, "Embedded license public key is invalid");
        return;
    }
    confirm_or_report_rollback();

    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    assert(gatt_server_init() == 0);
    assert(ble_svc_gap_device_name_set("LiteJam-RGBA1-Test") == 0);
    nimble_port_freertos_init(host_task);
}
