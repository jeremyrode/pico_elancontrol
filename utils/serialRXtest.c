#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"

#define BAUD_RATE 19200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

#define UART_TX_PIN 12
#define UART_RX_PIN 13
#define LED_PIN 25


// Globals for interrupt handlers
static int chars_rxed = 0;
static uint8_t receivedChars[35];   // an array to store the received data
static uint8_t lastBuffer[35];   // an array to store the received data
static int led = 0;

// Hardware UART RX interrupt handler
void on_uart_rx() {
  if (led == 0) {
    led = 1;
    gpio_put(LED_PIN, 1); // Toggle LED (testing)
  }
  else {
    led = 0;
    gpio_put(LED_PIN, 0); // Toggle LED (testing)
  }
  while (uart_is_readable(uart0)) {
    printf("%c",uart_getc(uart0));  // Just pass it through for now
  }
}



//Initialise everything
void initAll() {
  //Initialise I/O
  stdio_init_all();
  // initialise GPIO (Green LED connected to pin 25)
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
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
  // And set up and enable the interrupt handlers
  irq_set_exclusive_handler(UART0_IRQ, on_uart_rx);
  irq_set_enabled(UART0_IRQ, true);
  // Now enable the UART to send interrupts - RX only
  uart_set_irq_enables(uart0, true, false);
}

int main() {
  uint8_t in,channel,command;
  initAll();
  //Main Loop
  while(1) {
    in = getchar(); // Get the command from USB serial
    if (in == '1') {
      gpio_put(LED_PIN, 1); // Turn on LED (testing)
      led = 1;
      //printf("LED ON!\n"); //Test debug serial return
    }
    else if (in == '0') {
      gpio_put(LED_PIN, 0); // Turn on LED (testing)
      led = 0;
      //printf("LED OFF!\n");
    }
  }
}
