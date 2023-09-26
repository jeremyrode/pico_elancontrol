#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"

#define BAUD_RATE 9600
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

#define UART_TX_PIN 12
#define UART_RX_PIN 13
#define LED_PIN 25
#define MIN_CHANNEL 17
#define MAX_CHANNEL 22
// Bit Banging Parameters
#define PULSE_HIGH_WIDTH 12.5
#define SHORT_PERIOD 5
#define LONG_PERIOD 7.5

static int chars_rxed = 0; //malavikachugh@gmail.com  timothy.c.black@gmail.com
static uint8_t receivedChars[35];   // an array to store the received data
static uint8_t lastBuffer[35];   // an array to store the received data


int sendStatusBuffer() {
  bool identical = true;
  for (int rep=0; rep<sizeof(receivedChars); rep++) { //Todo exclude toggle bit
    if (receivedChars[rep] != lastBuffer[rep]) {
      identical = false;
      lastBuffer[rep] = receivedChars[rep]; //Copy changes
    }
  }
  if (!identical) {
    printf(receivedChars); //Send new status, this aint going to work
  }
}
// Serial RX interrupt handler
void on_uart_rx() {
  uint8_t cur_rx;
  while (uart_is_readable(uart0)) {
    cur_rx = uart_getc(uart0); // Get a char
    if (chars_rxed == 0 && cur_rx == 0xE0u) {
      chars_rxed = 1;
    }
    else if (chars_rxed == 1 && cur_rx == 0x00u) {
      chars_rxed = 2;
    }
    else if (chars_rxed == 2 && cur_rx == 0x81u) { //End of header
      chars_rxed = 3;
    }
    else if (chars_rxed > 3 && chars_rxed < 37) { //Data payload
        receivedChars[chars_rxed - 3] = cur_rx;
        chars_rxed++;
    }
    else if (chars_rxed == 37) { //End of data payload
      receivedChars[chars_rxed - 3] = cur_rx;
      sendStatusBuffer();
      chars_rxed = 0; //Reset counter
    }
    else {
      printf("Malformatted Status Message\n");
      chars_rxed = 0; //Reset counter
    }
  }
}

int sendZpadCommand(uint8_t channel, uint8_t command) {
  if ((channel > MAX_CHANNEL) || (channel < MIN_CHANNEL)) {
    printf("Channel Out Range: %d\n",channel);
    return(-1);
  }
  if ((command < 0) || (command > 63)) {
    printf("Command Out Range: %d\n",command);
    return(-1);
  }
  for (int rep=0; rep<5; rep++) {
    gpio_put(channel, 1);
    sleep_us(PULSE_HIGH_WIDTH);
    gpio_put(channel, 0);
    sleep_ms(SHORT_PERIOD);
  }
  for (int rep=0; rep<6; rep++) {
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

int initAll() {
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
      printf("LED ON!\n"); //Test debug serial return
    }
    else if (in == '0') {
      gpio_put(LED_PIN, 0); // Turn on LED (testing)
      printf("LED OFF!\n");
    }
    else if (in == 'S') { //ZPAD command
      channel = getchar();
      command = getchar();
      sendZpadCommand(channel,command);
    }
  }
}
