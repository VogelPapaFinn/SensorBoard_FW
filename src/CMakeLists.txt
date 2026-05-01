set(CMAKE_C_STANDARD 23)

set(FILES
        # Start of Application
        "main.cpp"

        # Drivers
        "Driver/Display.cpp"
        "Driver/Filesystem.cpp"
)

idf_component_register(SRCS ${FILES}
        PRIV_REQUIRES src driver spi_flash esp_psram esp_adc fatfs spiffs esp_wifi esp_http_server nvs_flash can esp_timer app_update esp_http_client esp_https_ota mbedtls esp-tls
        INCLUDE_DIRS "../include" "."
        EMBED_TXTFILES "" # For Files
        EMBED_FILES "") # For images

spiffs_create_partition_image(data "../data/" FLASH_INTO_PROJECT)

spiffs_create_partition_image(config "../config/" FLASH_INTO_PROJECT)