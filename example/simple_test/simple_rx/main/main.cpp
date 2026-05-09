#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define delay(ms) vTaskDelay(pdMS_TO_TICKS(ms))

#include "simple_rx.ino"


static void main_task(void *arg) {
  //printf("\nStart setup and loop\n");
  setup();
  while (1) {
    loop();
    vTaskDelay(pdMS_TO_TICKS(1)); // yield();
  }
}

extern "C" void app_main() {
  xTaskCreatePinnedToCore(
    &main_task,      // function
    "main_task",    // task name
    4096,           // stack size
    NULL,           // parameter
    5,              // priority
    NULL,           // task handle
    0               // core 0
  );
}

