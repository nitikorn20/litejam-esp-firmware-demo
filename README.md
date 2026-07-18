# LiteJam ESP Firmware Demo

Public, test-only firmware catalog for the LiteJam flashing-tool prototype.
Catalog schema v2 separates `Product → Hardware profile → Firmware version`
without publishing commercial product features.

## Repository layout

```text
catalog/catalog.json
products/<product-id>/product.svg
products/<product-id>/hardware/<hardware-profile-id>/releases/<version>/manifest.json
products/<product-id>/hardware/<hardware-profile-id>/releases/<version>/*.bin
source/test-firmware/
```

Current recommended profile: `guitar-rgba1 / esp32s3-16m-v1`, built for
ESP32-S3 with exactly 16 MB flash, dual 6 MB OTA slots, rollback and RSA-3072
signed test OTA images. The former ESP32 4 MB release remains available as a
separate legacy profile. Future products and MCU/capacity variants get new
folders; older releases remain immutable so the factory can roll back.

The ESP32-S3 profile was physically tested on a 16 MB ESP32-S3 (revision 0.2):
serial flash/verify, real BLE License binding/signature rejection, signed OTA,
unsigned-image rejection and bootloader rollback all passed. The NimBLE host
stack is explicitly 8 KB so RSA-3072 verification can finish inside the BLE
callback without overflowing the ESP-IDF default stack.

The downloadable package is attached to the matching GitHub Release. The
catalog stores its exact SHA-256 so the Flash Tool can reject corrupt or
unexpected downloads.

## Safety

- Test firmware only; do not ship to customers.
- No commercial guitar features are included.
- No private keys are committed.
- The embedded license public key is intentionally public.
- Hardware Secure Boot, Flash Encryption, and eFuse burning are not enabled.

Run `python tools/validate_manifest.py` to verify all binary sizes and hashes.
