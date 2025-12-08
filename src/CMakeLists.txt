set(CMAKE_C_STANDARD 23)

set(FILES
        # Start of Application
        "main.c"

        # Global stuff
        "../include/Global.h"
        "Global.c"
)

idf_component_register(SRCS ${FILES}
        PRIV_REQUIRES src driver spi_flash esp_psram esp_adc fatfs spiffs esp_wifi nvs_flash can logger esp_timer statemachine
        INCLUDE_DIRS "../include")