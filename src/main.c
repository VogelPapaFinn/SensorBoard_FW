#include "esp_log.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"

void app_main(void)
{
    twai_node_handle_t node_hdl = NULL;
    twai_onchip_node_config_t node_config = {
        .io_cfg.tx = GPIO_NUM_43,             // TWAI TX GPIO pin
        .io_cfg.rx = GPIO_NUM_2,             // TWAI RX GPIO pin
        .bit_timing.bitrate = 200000,  // 200 kbps bitrate
        .tx_queue_depth = 5,        // Transmit queue depth set to 5
    };
    // Create a new TWAI controller driver instance
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node_hdl));
    // Start the TWAI controller
    ESP_ERROR_CHECK(twai_node_enable(node_hdl));

    uint8_t send_buff[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    twai_frame_t tx_msg = {
        .header.id = 0x1,           // Message ID
        .header.ide = true,         // Use 29-bit extended ID format
        .buffer = send_buff,        // Pointer to data to transmit
        .buffer_len = sizeof(send_buff),  // Length of data to transmit
    };

    while (true)
    {
        printf("Transmitting\n");
        ESP_ERROR_CHECK(twai_node_transmit(node_hdl, &tx_msg, 0));  // Timeout = 0: returns immediately if queue is full

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
