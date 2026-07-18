from __future__ import annotations

import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def validate_manifest(path: Path) -> None:
    package = path.parent
    manifest = json.loads(path.read_text(encoding="utf-8-sig"))
    assert manifest["schema_version"] == 1
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
    assert catalog["schema_version"] == 1
    product_ids: set[str] = set()
    for product in catalog["products"]:
        assert product["product_id"] not in product_ids
        product_ids.add(product["product_id"])
        recommended = [release for release in product["releases"] if release["recommended"]]
        assert len(recommended) == 1
        assert recommended[0]["package_url"].startswith("https://github.com/")
        assert len(recommended[0]["package_sha256"]) == 64
    print(f"validated catalog with {len(product_ids)} product(s)")


if __name__ == "__main__":
    validate_catalog()
    manifests = sorted(ROOT.glob("products/*/releases/*/manifest.json"))
    assert manifests, "No release manifests found"
    for manifest_path in manifests:
        validate_manifest(manifest_path)
