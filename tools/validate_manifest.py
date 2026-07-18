from __future__ import annotations

import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def validate_manifest(path: Path) -> None:
    package = path.parent
    manifest = json.loads(path.read_text(encoding="utf-8-sig"))
    assert manifest["schema_version"] in {1, 2}
    if manifest["schema_version"] == 2:
        assert manifest["hardware_profile_id"]
        assert manifest["firmware_id"]
        assert manifest["expected_flash_size"] >= manifest["minimum_flash_size"]
        assert manifest["ota"]["scheme"] == "dual-slot"
        assert manifest["ota"]["slot_size_bytes"] > 0
        assert manifest["ota"]["signing_key_id"]
    assert manifest["release_channel"] in {"test", "production"}
    assert manifest["health_check"]["expected_product_id"] == manifest["product_id"]
    assert manifest["health_check"]["expected_firmware_version"] == manifest["firmware_version"]
    addresses: set[str] = set()
    for image in manifest["images"]:
        assert image["address"] not in addresses
        addresses.add(image["address"])
        binary = package / image["file"]
        data = binary.read_bytes()
        assert len(data) == image["size"], f"size mismatch: {binary}"
        digest = hashlib.sha256(data).hexdigest()
        assert digest == image["sha256"], f"SHA-256 mismatch: {binary}"
    print(f"validated {manifest['package_id']}")


def validate_catalog() -> None:
    catalog = json.loads((ROOT / "catalog" / "catalog.json").read_text(encoding="utf-8"))
    assert catalog["schema_version"] == 2
    product_ids: set[str] = set()
    for product in catalog["products"]:
        assert product["product_id"] not in product_ids
        product_ids.add(product["product_id"])
        profile_ids: set[str] = set()
        recommended_profiles = [profile for profile in product["hardware_profiles"] if profile["recommended"]]
        assert len(recommended_profiles) == 1
        for profile in product["hardware_profiles"]:
            assert profile["hardware_profile_id"] not in profile_ids
            profile_ids.add(profile["hardware_profile_id"])
            assert profile["target_chip"]
            assert profile["flash_size_bytes"] > 0
            recommended = [release for release in profile["releases"] if release["recommended"]]
            assert len(recommended) == 1
            assert recommended[0]["package_url"].startswith("https://github.com/")
            assert len(recommended[0]["package_sha256"]) == 64
    print(f"validated catalog with {len(product_ids)} product(s)")


if __name__ == "__main__":
    validate_catalog()
    manifests = sorted(ROOT.glob("products/*/releases/*/manifest.json"))
    manifests += sorted(ROOT.glob("products/*/hardware/*/releases/*/manifest.json"))
    assert manifests, "No release manifests found"
    for manifest_path in manifests:
        validate_manifest(manifest_path)
