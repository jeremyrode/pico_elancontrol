#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"

#define UART_ID uart0
#define BAUD_RATE 9600
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

// We are using pins 0 and 1, but see the GPIO function select table in the
// datasheet for information on which other pins can be used.
#define UART_TX_PIN 12
#define UART_RX_PIN 13
#define LED_PIN 25
#define MIN_CHANNEL 17
#define MAX_CHANNEL 22
// Bit Banging Parameters
#define PULSE_HIGH_WIDTH 12.5
#define SHORT_PERIOD 5
#define LONG_PERIOD 7.5

static int chars_rxed = 0;

// Serial RX interrupt handler
void on_uart_rx() {
  while (uart_is_readable(UART_ID)) {
    uint8_t ch = uart_getc(UART_ID);
    printf("Got %c, %d total\n",ch,chars_rxed);
    chars_rxed++;
  }
}

int sendZpadCommand(int channel, int command) {
  if (channel > MAX_CHANNEL) || (channel < MIN_CHANNEL) {
    printf("Channel Out Range: %d",channel);
    return(-1);
  }
  for (rep=0; rep<5; rep++) {
    gpio_put(channel, 1);
    sleep_us(PULSE_HIGH_WIDTH);
    gpio_put(channel, 0);
    sleep_ms(SHORT_PERIOD);
  }
  for (rep=0; rep<6; rep++) {
    gpio_put(channel, 1);
    sleep_us(PULSE_HIGH_WIDTH);
    gpio_put(channel, 0);
    if (command & 1<<(5-rep)) {
      sleep_ms(LONG_PERIOD);
    }
    else {
      sleep_ms(SHORT_PERIOD);
    }
  }
  gpio_put(channel, 1);
  sleep_us(PULSE_HIGH_WIDTH);
  gpio_put(channel, 0);
}

int main() {
  char in;
  //Initialise I/O
  stdio_init_all();
  // initialise GPIO (Green LED connected to pin 25)
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  // Set up our UART with a basic baud rate.
  uart_init(UART_ID, 2400);
  // Set the TX and RX pins by using the function select on the GPIO
  gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
  // The call will return the actual baud rate selected
  int __unused actual = uart_set_baudrate(UART_ID, BAUD_RATE);
  // Set UART flow control CTS/RTS, we don't want these, so turn them off
  uart_set_hw_flow(UART_ID, false, false);
  // Set our data format
  uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
  // Turn off FIFO's - we want to do this character by character
  uart_set_fifo_enabled(UART_ID, false);
  // Select correct interrupt for the UART we are using
  int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;
  // And set up and enable the interrupt handlers
  irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
  irq_set_enabled(UART_IRQ, true);
  // Now enable the UART to send interrupts - RX only
  uart_set_irq_enables(UART_ID, true, false);


  //Main Loop
  while(1) {
    in = getchar();
    if (in == '1') {
      gpio_put(LED_PIN, 1); // Set pin 25 to high
      printf("LED ON!\n");
    }
    else if (in == '0') {
      gpio_put(LED_PIN, 0); // Set pin 25 to low
      printf("LED OFF!\n");
    }
  }
}
