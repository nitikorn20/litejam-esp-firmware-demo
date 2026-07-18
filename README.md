# LiteJam ESP Firmware Demo

Public, test-only firmware catalog for the LiteJam flashing-tool prototype.
It demonstrates how multiple products and historical firmware versions are
separated without publishing commercial product features.

## Repository layout

```text
catalog/catalog.json
products/<product-id>/product.svg
products/<product-id>/releases/<version>/manifest.json
products/<product-id>/releases/<version>/*.bin
source/test-firmware/
```

Current product: `guitar-rgba1`, color `#FF6B35`, firmware
`0.1.0-test`. Future products get a new folder; older releases remain under
their own version folder so the factory can roll back without replacing files.

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
