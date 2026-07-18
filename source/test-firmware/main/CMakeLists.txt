idf_component_register(
  SRCS
    "main.c"
    "gatt_server.c"
    "license_verify.c"
  INCLUDE_DIRS "."
  REQUIRES
    app_update
    bt
    esp_hw_support
    json
    mbedtls
    nvs_flash
    spi_flash
  EMBED_TXTFILES "license_test_public.pem"
)
