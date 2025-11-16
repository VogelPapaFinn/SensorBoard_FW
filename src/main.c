// Project includes
#include "can.h"

void app_main(void)
{
  ESP_ERROR_CHECK(initializeCanNode(GPIO_NUM_43, GPIO_NUM_2) == NULL);

  ESP_ERROR_CHECK(enableCanNode() == false);

  uint8_t send_buff[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  twai_frame_t frame = {
    .header.id = 0x1,           // Message ID
    .header.ide = true,         // Use 29-bit extended ID format
    .buffer = send_buff,        // Pointer to data to transmit
    .buffer_len = sizeof(send_buff),  // Length of data to transmit
  };
  while (1) {
    printf("Transmitted\n");
    queueCanBusMessage(&frame, false);
    vTaskDelay(pdMS_TO_TICKS(2500));
  }
}
