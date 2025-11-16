set(CMAKE_C_STANDARD 23)

set(FILES
        # Start of Application
        "main.c"
)

idf_component_register(SRCS ${FILES}
        PRIV_REQUIRES src driver spi_flash esp_psram esp_adc fatfs spiffs can
        INCLUDE_DIRS "../include")