# ESP32-S3 16 MB hardware profile

Stable profile ID: `esp32s3-16m-v1`

- Target: ESP32-S3
- Exact flash capacity: 16 MB
- OTA: two 6 MB slots with rollback
- OTA signature: RSA-3072 test key (`ota-test-s3-rsa3072-2026-01`)
- License verification: shared ECDSA P-256 test public key embedded in `app.bin`
- Reserved factory data: 64 KB, read-only to normal partition APIs
- General storage: 3.8125 MB
- Coredump: 64 KB

The private OTA test key is local and gitignored. Production private keys must
live in a secret manager or isolated signing system and must never be copied
into this repository, the flasher, firmware packages, or the mobile app.

Build with the Windows ESP-IDF lane:

```powershell
. C:\Espressif\tools\Microsoft.v5.4.4.PowerShell_profile.ps1
idf.py -B build-win-esp32s3-16m-v1 -D SDKCONFIG="profiles/esp32s3-16m-v1/sdkconfig.generated" -D SDKCONFIG_DEFAULTS="profiles/esp32s3-16m-v1/sdkconfig.defaults" set-target esp32s3
idf.py -B build-win-esp32s3-16m-v1 build
```
