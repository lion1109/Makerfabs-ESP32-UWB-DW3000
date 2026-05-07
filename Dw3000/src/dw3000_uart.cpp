/*
 * UART.c
 *
 * Created: 9/10/2021 12:32:14 PM
 *  Author: Emin Eminof
 */ 
#include "dw3000_uart.h"


#if USE_ARDUINO

void UART_init(void)
{
  Serial.begin(115200);
}

void UART_putc(char data)
{
  Serial.print(data);
}

void UART_puts(char* s)
{
  Serial.print(s);
}

#else

#include <driver/uart.h>

#define UART_PORT_NUM      UART_NUM_0   // UART0 ist für Konsole/USB
#define UART_BAUD_RATE     115200
#define UART_TX_PIN        1            // TX Pin (GPIO1 für UART0)
#define UART_RX_PIN        3            // RX Pin (GPIO3 für UART0)

void UART_init() {
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        //.source_clk = SOC_MOD_CLK_CPU
	//.flags = 0,
	//.rx_flow_ctrl_thresh = 0
    };
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_PORT_NUM, 1024, 0, 0, nullptr, 0);
}

void UART_putc(char c) {
    uart_write_bytes(UART_PORT_NUM, &c, 1);
}

void UART_puts(const char* s) {
    uart_write_bytes(UART_PORT_NUM, s, strlen(s));
}

#endif

void test_run_info(unsigned char * s)
{
    UART_puts((char *)s);
    UART_puts("\r\n");
}
