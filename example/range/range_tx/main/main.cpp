#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define delay(ms) vTaskDelay(pdMS_TO_TICKS(ms))

class IO_Device {
        public: inline IO_Device() {}
        public: inline void println(const char *s) { printf("%s\n", s); }
};

IO_Device Serial;

#include "range_tx.ino"


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

