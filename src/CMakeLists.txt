set(CMAKE_C_STANDARD 23)

set(FILES
        # Start of Application
        "main.c"

        # Webinterface
        "../include/WebInterface.h"
        "WebInterface.c"
        "../include/WebAPI.h"
        "WebAPI.c"

        # Global stuff
        "../include/Global.h"
        "Global.c"

        # Data Center
        "../include/DataCenter.h"
        "DataCenter.c"
)

idf_component_register(SRCS ${FILES}
        PRIV_REQUIRES src driver spi_flash esp_psram esp_adc fatfs spiffs esp_wifi esp_http_server nvs_flash can logger esp_timer statemachine
        INCLUDE_DIRS "../include" "."
        EMBED_TXTFILES "" # For Files
        EMBED_FILES "") # For images

spiffs_create_partition_image(resources "../res/" FLASH_INTO_PROJECT)