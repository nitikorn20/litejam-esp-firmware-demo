#include "gatt_server.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_ota_ops.h"
#include "esp_flash.h"
#include "host/ble_att.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "license_verify.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "sdkconfig.h"

#define UUID_LITEJAM_SERVICE 0xfff0
#define UUID_DEVICE_INFO 0xfff1
#define UUID_LICENSE_SUBMIT 0xfff2
#define UUID_LICENSE_STATUS 0xfff3
#define UUID_BASIC_COMMAND 0xfff4
#define UUID_PREMIUM_COMMAND 0xfff5
#define UUID_OTA_CONTROL 0xfff6
#define UUID_OTA_DATA 0xfff7
#define UUID_OTA_STATUS 0xfff8

#define PRODUCT_ID "guitar-rgba1"
#define FIRMWARE_VERSION "0.1.0-test"
#define PROTOCOL_VERSION 1
#define LICENSE_SCHEMA_VERSION 1
#define LICENSE_KEY_ID "license-test-2026-01"
#define MAX_WRITE_LENGTH 768

static const char *TAG = "litejam_gatt";
static bool premium_unlocked;
static char license_status[40] = "LOCKED:NO_LICENSE";
static bool ota_in_progress;
static esp_ota_handle_t ota_handle;
static const esp_partition_t *ota_partition;
static size_t ota_bytes_written;
static char ota_status[64] = "IDLE";

static void factory_mac_string(char output[13])
{
    uint8_t mac[6] = {0};
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));
    snprintf(
        output,
        13,
        "%02x%02x%02x%02x%02x%02x",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
    );
}

static int append_text(struct os_mbuf *output, const char *text)
{
    return os_mbuf_append(output, text, strlen(text)) == 0
               ? 0
               : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int copy_write(struct os_mbuf *input, uint8_t *destination, size_t capacity, uint16_t *length)
{
    uint16_t input_length = OS_MBUF_PKTLEN(input);
    if (input_length == 0 || input_length > capacity) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    return ble_hs_mbuf_to_flat(input, destination, (uint16_t)capacity, length) == 0
               ? 0
               : BLE_ATT_ERR_UNLIKELY;
}

static int read_device_info(struct ble_gatt_access_ctxt *context)
{
    char mac[13];
    char response[480];
    uint32_t flash_size = 0;
    factory_mac_string(mac);
    ESP_ERROR_CHECK(esp_flash_get_physical_size(esp_flash_default_chip, &flash_size));
    snprintf(
        response,
        sizeof(response),
        "{\"factory_mac\":\"%s\",\"product_id\":\"%s\","
        "\"hardware_profile_id\":\"%s\",\"firmware_id\":\"%s\","
        "\"target_chip\":\"%s\",\"flash_size_bytes\":%lu,"
        "\"firmware_version\":\"%s\",\"protocol_version\":%d,"
        "\"license_schema_version\":%d,\"license_key_id\":\"%s\","
        "\"capabilities\":[\"basic_demo\",\"premium_demo\",\"ble_ota\",\"signed_ota\"]}",
        mac,
        PRODUCT_ID,
        CONFIG_LITEJAM_HARDWARE_PROFILE_ID,
        CONFIG_LITEJAM_FIRMWARE_ID,
        CONFIG_IDF_TARGET,
        (unsigned long)flash_size,
        FIRMWARE_VERSION,
        PROTOCOL_VERSION,
        LICENSE_SCHEMA_VERSION,
        LICENSE_KEY_ID
    );
    return append_text(context->om, response);
}

static int submit_license(struct ble_gatt_access_ctxt *context)
{
    uint8_t envelope[MAX_WRITE_LENGTH];
    uint16_t envelope_length = 0;
    int result = copy_write(context->om, envelope, sizeof(envelope), &envelope_length);
    if (result != 0) {
        return result;
    }
    char mac[13];
    factory_mac_string(mac);
    license_verify_result_t verified = license_verify_envelope(
        envelope,
        envelope_length,
        mac,
        "premium_demo"
    );
    premium_unlocked = verified == LICENSE_VERIFY_OK;
    snprintf(
        license_status,
        sizeof(license_status),
        "%s:%s",
        premium_unlocked ? "UNLOCKED" : "LOCKED",
        license_verify_result_name(verified)
    );
    ESP_LOGI(TAG, "License result: %s", license_status);
    return premium_unlocked ? 0 : BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
}

static int ota_control(struct ble_gatt_access_ctxt *context)
{
    uint8_t command[32] = {0};
    uint16_t command_length = 0;
    int result = copy_write(context->om, command, sizeof(command) - 1, &command_length);
    if (result != 0) {
        return result;
    }
    command[command_length] = '\0';
    if (strcmp((char *)command, "BEGIN") == 0) {
        if (ota_in_progress) {
            return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
        }
        ota_partition = esp_ota_get_next_update_partition(NULL);
        if (ota_partition == NULL ||
            esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle) != ESP_OK) {
            snprintf(ota_status, sizeof(ota_status), "ERR_BEGIN");
            return BLE_ATT_ERR_UNLIKELY;
        }
        ota_in_progress = true;
        ota_bytes_written = 0;
        snprintf(ota_status, sizeof(ota_status), "RECEIVING:0");
        return 0;
    }
    if (strcmp((char *)command, "ABORT") == 0) {
        if (ota_in_progress) {
            esp_ota_abort(ota_handle);
        }
        ota_in_progress = false;
        snprintf(ota_status, sizeof(ota_status), "ABORTED");
        return 0;
    }
    if (strcmp((char *)command, "END") == 0) {
        if (!ota_in_progress) {
            return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
        }
        esp_err_t end_result = esp_ota_end(ota_handle);
        ota_in_progress = false;
        if (end_result != ESP_OK || esp_ota_set_boot_partition(ota_partition) != ESP_OK) {
            snprintf(ota_status, sizeof(ota_status), "ERR_VERIFY");
            return BLE_ATT_ERR_UNLIKELY;
        }
        snprintf(ota_status, sizeof(ota_status), "READY_REBOOT:%u", (unsigned)ota_bytes_written);
        return 0;
    }
    return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
}

static int ota_data(struct ble_gatt_access_ctxt *context)
{
    if (!ota_in_progress) {
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    }
    uint16_t length = OS_MBUF_PKTLEN(context->om);
    if (length == 0 || length > 512) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    uint8_t data[512];
    if (ble_hs_mbuf_to_flat(context->om, data, sizeof(data), NULL) != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    if (esp_ota_write(ota_handle, data, length) != ESP_OK) {
        snprintf(ota_status, sizeof(ota_status), "ERR_WRITE");
        return BLE_ATT_ERR_UNLIKELY;
    }
    ota_bytes_written += length;
    snprintf(ota_status, sizeof(ota_status), "RECEIVING:%u", (unsigned)ota_bytes_written);
    return 0;
}

static int access_characteristic(
    uint16_t connection_handle,
    uint16_t attribute_handle,
    struct ble_gatt_access_ctxt *context,
    void *argument)
{
    (void)connection_handle;
    (void)attribute_handle;
    (void)argument;
    uint16_t uuid = ble_uuid_u16(context->chr->uuid);
    switch (uuid) {
    case UUID_DEVICE_INFO:
        return read_device_info(context);
    case UUID_LICENSE_SUBMIT:
        return submit_license(context);
    case UUID_LICENSE_STATUS:
        return append_text(context->om, license_status);
    case UUID_BASIC_COMMAND:
        ESP_LOGI(TAG, "Basic demo command accepted");
        return 0;
    case UUID_PREMIUM_COMMAND:
        if (!premium_unlocked) {
            return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
        }
        ESP_LOGI(TAG, "Premium demo command accepted");
        return 0;
    case UUID_OTA_CONTROL:
        return ota_control(context);
    case UUID_OTA_DATA:
        return ota_data(context);
    case UUID_OTA_STATUS:
        return append_text(context->om, ota_status);
    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

static const struct ble_gatt_svc_def services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(UUID_LITEJAM_SERVICE),
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = BLE_UUID16_DECLARE(UUID_DEVICE_INFO),
                .access_cb = access_characteristic,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                .uuid = BLE_UUID16_DECLARE(UUID_LICENSE_SUBMIT),
                .access_cb = access_characteristic,
                .flags = BLE_GATT_CHR_F_WRITE,
            },
            {
                .uuid = BLE_UUID16_DECLARE(UUID_LICENSE_STATUS),
                .access_cb = access_characteristic,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                .uuid = BLE_UUID16_DECLARE(UUID_BASIC_COMMAND),
                .access_cb = access_characteristic,
                .flags = BLE_GATT_CHR_F_WRITE,
            },
            {
                .uuid = BLE_UUID16_DECLARE(UUID_PREMIUM_COMMAND),
                .access_cb = access_characteristic,
                .flags = BLE_GATT_CHR_F_WRITE,
            },
            {
                .uuid = BLE_UUID16_DECLARE(UUID_OTA_CONTROL),
                .access_cb = access_characteristic,
                .flags = BLE_GATT_CHR_F_WRITE,
            },
            {
                .uuid = BLE_UUID16_DECLARE(UUID_OTA_DATA),
                .access_cb = access_characteristic,
                .flags = BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE,
            },
            {
                .uuid = BLE_UUID16_DECLARE(UUID_OTA_STATUS),
                .access_cb = access_characteristic,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {0},
        },
    },
    {0},
};

int gatt_server_init(void)
{
    ble_svc_gap_init();
    ble_svc_gatt_init();
    int result = ble_gatts_count_cfg(services);
    return result == 0 ? ble_gatts_add_svcs(services) : result;
}

void gatt_server_on_disconnect(void)
{
    premium_unlocked = false;
    snprintf(license_status, sizeof(license_status), "LOCKED:DISCONNECTED");
    if (ota_in_progress) {
        esp_ota_abort(ota_handle);
        ota_in_progress = false;
        snprintf(ota_status, sizeof(ota_status), "ABORTED:DISCONNECTED");
    }
}
