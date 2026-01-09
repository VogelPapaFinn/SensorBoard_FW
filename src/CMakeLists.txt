set(CMAKE_C_STANDARD 23)

set(FILES
        # Start of Application
        "main.c"

        # Webinterface
        "../include/WebInterface/WebInterface.h"
        "WebInterface/WebInterface.c"
        "../include/WebInterface/WebAPI.h"
        "WebInterface/WebAPI.c"

        # Config Manager
        "../include/ConfigManager.h"
        "ConfigManager.c"

        # Display Manager
        "../include/DisplayManager.h"
        "DisplayManager.c"

        # File Manager
        "../include/FilesystemDriver.h"
        "FilesystemDriver.c"

        # Queue System
        "../include/EventQueues.h"
        "EventQueues.c"

        # Sensor Manager
        "../include/SensorManager.h"
        "SensorManager.c"

        # Display Update
        "../include/DisplayUpdate.h"
        "DisplayUpdate.c"

        # Sensors
        "../include/Sensors/FuelLevelSensor.h"
        "../include/Sensors/IndicatorsSensor.h"
        "../include/Sensors/InternalTemperatureSensor.h"
        "../include/Sensors/OilPressureSensor.h"
        "../include/Sensors/RpmSensor.h"
        "../include/Sensors/SpeedSensor.h"
        "../include/Sensors/WaterTemperatureSensor.h"
        "Sensors/FuelLevelSensor.c"
        "Sensors/IndicatorsSensor.c"
        "Sensors/InternalTemperatureSensor.c"
        "Sensors/OilPressureSensor.c"
        "Sensors/RpmSensor.c"
        "Sensors/SpeedSensor.c"
        "Sensors/WaterTemperatureSensor.c"

        # Sensor Utils
        "../include/Sensors/SensorUtils.h"
        "Sensors/SensorUtils.c"

        # Wifi Driver
        "../include/WifiDriver.h"
        "WifiDriver.c"
)

idf_component_register(SRCS ${FILES}
        PRIV_REQUIRES src driver spi_flash esp_psram esp_adc fatfs spiffs esp_wifi esp_http_server nvs_flash can esp_timer
        INCLUDE_DIRS "../include" "."
        EMBED_TXTFILES  "" # For Files
        EMBED_FILES "") # For images

spiffs_create_partition_image(data "../data/" FLASH_INTO_PROJECT)

spiffs_create_partition_image(config "../config/" FLASH_INTO_PROJECT)