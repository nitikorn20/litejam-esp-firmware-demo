# Guitar RGBA1

- Product ID: `guitar-rgba1`
- Factory color: orange `#FF6B35`
- Recommended target: ESP32-S3 with 16 MB flash (`esp32s3-16m-v1`)
- Legacy target: ESP32 with 4 MB flash (`esp32-4m-v1`)
- Public profile: BLE device information, signed license POC, and signed BLE OTA

Each release is immutable after publication. If a firmware correction is
needed, create a new version folder and update `catalog/catalog.json`; do not
replace binaries in an existing folder.

Never reuse a hardware profile ID after changing MCU, flash capacity, flash
addresses, partition layout, or boot requirements. Create a new profile ID and
keep the former profile and versions intact.
