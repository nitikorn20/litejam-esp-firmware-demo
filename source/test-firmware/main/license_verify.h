#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    LICENSE_VERIFY_OK = 0,
    LICENSE_VERIFY_FORMAT_ERROR,
    LICENSE_VERIFY_SIGNATURE_ERROR,
    LICENSE_VERIFY_DEVICE_MISMATCH,
    LICENSE_VERIFY_PRODUCT_MISMATCH,
    LICENSE_VERIFY_KEY_MISMATCH,
    LICENSE_VERIFY_FEATURE_MISSING,
} license_verify_result_t;

bool license_verify_public_key_ready(void);

license_verify_result_t license_verify_envelope(
    const uint8_t *envelope,
    size_t envelope_length,
    const char *factory_mac,
    const char *required_feature
);

const char *license_verify_result_name(license_verify_result_t result);
