#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"

//Hardware UART Setup
#define BAUD_RATE 19200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

#define UART_TX_PIN 12
#define UART_RX_PIN 13
#define LED_PIN 25


void on_uart_rx() {
  char cur_rx;
  gpio_put(LED_PIN, !gpio_get(LED_PIN)); // Toggle LED on update sent
  putchar_raw('R');
  while (uart_is_readable(uart0)) {
    cur_rx = uart_getc(uart0); // Get a char
    putchar_raw(cur_rx); // pass it along
  }
}

//Initialise everything
void initAll() {
  //Initialise I/O
  stdio_usb_init();
  // initialise GPIO (Green LED connected to pin 25)
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  //Serial Section
  // Set up our UART with a basic baud rate.
  uart_init(uart0, 2400);
  // Set the TX and RX pins by using the function select on the GPIO
  gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
  // The call will return the actual baud rate selected
  int __unused actual = uart_set_baudrate(uart0, BAUD_RATE);
  // Set UART flow control CTS/RTS, we don't want these, so turn them off
  uart_set_hw_flow(uart0, false, false);
  // Set our data format
  uart_set_format(uart0, DATA_BITS, STOP_BITS, PARITY);
  // Turn on FIFOs
  uart_set_fifo_enabled(uart0, true);
  uart_set_translate_crlf	(uart0, false);
  // And set up and enable the interrupt handlers
  irq_set_exclusive_handler(UART0_IRQ, on_uart_rx);
  irq_set_enabled(UART0_IRQ, true);
  // Now enable the UART to send interrupts - RX only
  uart_set_irq_enables(uart0, true, false);
}


int main() {
  uint32_t in;
  initAll();
  while (1) {
    in = getchar_timeout_us(10000000); // Get the command from USB serial
    switch (in) {
      case PICO_ERROR_TIMEOUT:
        printf("Alive");
      break;
      default:
        printf("Invalid Command");
    }
  }
}

