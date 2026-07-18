#include "license_verify.h"

#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "mbedtls/base64.h"
#include "mbedtls/md.h"
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"

#define PRODUCT_ID "guitar-rgba1"
#define LICENSE_KEY_ID "license-test-2026-01"
#define MAX_LICENSE_ENVELOPE 768
#define MAX_SIGNATURE_DER 96

extern const uint8_t license_public_key_start[] asm("_binary_license_test_public_pem_start");
extern const uint8_t license_public_key_end[] asm("_binary_license_test_public_pem_end");

static bool parse_public_key(mbedtls_pk_context *context)
{
    mbedtls_pk_init(context);
    size_t length = (size_t)(license_public_key_end - license_public_key_start);
    return mbedtls_pk_parse_public_key(context, license_public_key_start, length) == 0;
}

bool license_verify_public_key_ready(void)
{
    mbedtls_pk_context context;
    bool ready = parse_public_key(&context);
    mbedtls_pk_free(&context);
    return ready;
}

static bool json_string_equals(cJSON *root, const char *name, const char *expected)
{
    cJSON *value = cJSON_GetObjectItemCaseSensitive(root, name);
    return cJSON_IsString(value) && value->valuestring != NULL &&
           strcmp(value->valuestring, expected) == 0;
}

static bool json_features_contains(cJSON *root, const char *required_feature)
{
    cJSON *features = cJSON_GetObjectItemCaseSensitive(root, "features");
    if (!cJSON_IsArray(features)) {
        return false;
    }
    cJSON *feature = NULL;
    cJSON_ArrayForEach(feature, features) {
        if (cJSON_IsString(feature) && feature->valuestring != NULL &&
            strcmp(feature->valuestring, required_feature) == 0) {
            return true;
        }
    }
    return false;
}

license_verify_result_t license_verify_envelope(
    const uint8_t *envelope,
    size_t envelope_length,
    const char *factory_mac,
    const char *required_feature)
{
    if (envelope == NULL || factory_mac == NULL || required_feature == NULL ||
        envelope_length == 0 || envelope_length > MAX_LICENSE_ENVELOPE) {
        return LICENSE_VERIFY_FORMAT_ERROR;
    }

    const uint8_t *separator = NULL;
    for (size_t index = envelope_length; index > 0; --index) {
        if (envelope[index - 1] == '\n') {
            separator = &envelope[index - 1];
            break;
        }
    }
    if (separator == NULL || separator == envelope || separator == &envelope[envelope_length - 1]) {
        return LICENSE_VERIFY_FORMAT_ERROR;
    }

    const size_t payload_length = (size_t)(separator - envelope);
    const uint8_t *signature_b64 = separator + 1;
    const size_t signature_b64_length = envelope_length - payload_length - 1;
    uint8_t signature[MAX_SIGNATURE_DER];
    size_t signature_length = 0;
    if (mbedtls_base64_decode(
            signature,
            sizeof(signature),
            &signature_length,
            signature_b64,
            signature_b64_length) != 0) {
        return LICENSE_VERIFY_FORMAT_ERROR;
    }

    uint8_t digest[32];
    if (mbedtls_sha256(envelope, payload_length, digest, 0) != 0) {
        return LICENSE_VERIFY_SIGNATURE_ERROR;
    }
    mbedtls_pk_context public_key;
    if (!parse_public_key(&public_key)) {
        return LICENSE_VERIFY_SIGNATURE_ERROR;
    }
    int verification = mbedtls_pk_verify(
        &public_key,
        MBEDTLS_MD_SHA256,
        digest,
        sizeof(digest),
        signature,
        signature_length
    );
    mbedtls_pk_free(&public_key);
    if (verification != 0) {
        return LICENSE_VERIFY_SIGNATURE_ERROR;
    }

    cJSON *root = cJSON_ParseWithLength((const char *)envelope, payload_length);
    if (root == NULL || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return LICENSE_VERIFY_FORMAT_ERROR;
    }
    license_verify_result_t result = LICENSE_VERIFY_OK;
    cJSON *schema = cJSON_GetObjectItemCaseSensitive(root, "schema_version");
    if (!cJSON_IsNumber(schema) || schema->valueint != 1) {
        result = LICENSE_VERIFY_FORMAT_ERROR;
    } else if (!json_string_equals(root, "device_mac", factory_mac)) {
        result = LICENSE_VERIFY_DEVICE_MISMATCH;
    } else if (!json_string_equals(root, "product_id", PRODUCT_ID)) {
        result = LICENSE_VERIFY_PRODUCT_MISMATCH;
    } else if (!json_string_equals(root, "key_id", LICENSE_KEY_ID)) {
        result = LICENSE_VERIFY_KEY_MISMATCH;
    } else if (!json_features_contains(root, required_feature)) {
        result = LICENSE_VERIFY_FEATURE_MISSING;
    }
    cJSON_Delete(root);
    return result;
}

const char *license_verify_result_name(license_verify_result_t result)
{
    switch (result) {
    case LICENSE_VERIFY_OK:
        return "OK";
    case LICENSE_VERIFY_FORMAT_ERROR:
        return "ERR_FORMAT";
    case LICENSE_VERIFY_SIGNATURE_ERROR:
        return "ERR_SIGNATURE";
    case LICENSE_VERIFY_DEVICE_MISMATCH:
        return "ERR_DEVICE_MISMATCH";
    case LICENSE_VERIFY_PRODUCT_MISMATCH:
        return "ERR_PRODUCT_MISMATCH";
    case LICENSE_VERIFY_KEY_MISMATCH:
        return "ERR_KEY_MISMATCH";
    case LICENSE_VERIFY_FEATURE_MISSING:
        return "ERR_FEATURE_MISSING";
    default:
        return "ERR_UNKNOWN";
    }
}
