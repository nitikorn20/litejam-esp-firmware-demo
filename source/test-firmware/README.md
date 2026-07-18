# LiteJam Guitar RGBA1 Test Firmware

Public-safe ESP-IDF 5.4 test firmware for an ESP32-WROOM-32E (revision 3 or
newer). It contains no commercial guitar features.

Implemented proof points:

- Factory MAC and firmware/protocol information over BLE
- Basic command available without a license
- Premium command blocked until a device-bound ECDSA P-256 license verifies
- BLE OTA transport with two OTA slots
- Signed OTA verification without hardware Secure Boot
- Boot rollback confirmation after the embedded public-key self-check

## Key separation

- `main/license_test_public.pem` is safe to distribute and is embedded in
  `app.bin` for license verification.
- `keys/ota_test_signing_key.pem` signs test OTA images and is gitignored.
- The license private key stays under `mock-server/.secrets/` and is never
  copied into this project.

Both key pairs are test-only and must be replaced for production.

## BLE contract

Service `0xFFF0`:

| Characteristic | Access | Purpose |
| --- | --- | --- |
| `0xFFF1` | Read | DEVICE_INFO JSON |
| `0xFFF2` | Write | LICENSE_SUBMIT |
| `0xFFF3` | Read | LICENSE_STATUS |
| `0xFFF4` | Write | BASIC_DEMO_COMMAND |
| `0xFFF5` | Write | PREMIUM_DEMO_COMMAND |
| `0xFFF6` | Write | OTA control: `BEGIN`, `END`, `ABORT` |
| `0xFFF7` | Write | OTA image chunks, maximum 512 bytes |
| `0xFFF8` | Read | OTA status |

Write the mock server's `ble_envelope` value to `0xFFF2`. It is the exact
canonical JSON payload, one newline, then the base64 DER signature. Unlock is
per BLE session and is cleared at disconnect.

Monthly expiry is enforced by the official app/server renewal policy because
this board has no trusted wall clock. Firmware independently verifies the
signature, Factory MAC, product, key id, and requested feature.

## Build only (does not flash hardware)

```powershell
. C:\Espressif\tools\Microsoft.v5.4.4.PowerShell_profile.ps1
espsecure.py generate_signing_key --version 1 keys\ota_test_signing_key.pem
idf.py -B build-win set-target esp32
idf.py -B build-win build
```

The key generator is needed only once. Never run `idf.py flash` as part of an
automated test.
