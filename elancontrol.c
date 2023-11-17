#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include <sendzpadcommand.pio.h>

#define BAUD_RATE 19200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

#define UART_TX_PIN 12
#define UART_RX_PIN 13
#define LED_PIN 25
//38 bytes in status with 3 bytes as header (not stored)
#define STATUS_LEN 35
// PIO Locations
#define CHANNEL_1_PIN 17
#define CHANNEL_2_PIN 18
#define CHANNEL_3_PIN 19
#define CHANNEL_4_PIN 20
#define CHANNEL_5_PIN 21
#define CHANNEL_6_PIN 22

#define CHANNEL_1_PIO pio0
#define CHANNEL_2_PIO pio0
#define CHANNEL_3_PIO pio0
#define CHANNEL_4_PIO pio0
#define CHANNEL_5_PIO pio1
#define CHANNEL_6_PIO pio1

#define CHANNEL_1_SMID 0
#define CHANNEL_2_SMID 1
#define CHANNEL_3_SMID 2
#define CHANNEL_4_SMID 3
#define CHANNEL_5_SMID 0
#define CHANNEL_6_SMID 1

// Globals for interrupt handlers
static int chars_rxed = 0;
static uint8_t receivedChars[STATUS_LEN];   // an array to store the received data
static uint8_t lastBuffer[STATUS_LEN];   // an array to store the received data



// Handle a complete RX Status Buffer
int sendStatusBuffer() {
  bool identical = true;
  for (int rep=0; rep<sizeof(receivedChars); rep++) { //Todo exclude toggle bit
    if (receivedChars[rep] != lastBuffer[rep]) {
      identical = false;
      lastBuffer[rep] = receivedChars[rep]; //Copy changes
    }
  }
  if (!identical) {
    printf("NB: %.*s\n", STATUS_LEN, receivedChars); //Send new status, len as no null term
  }
}



// Hardware UART RX interrupt handler
void on_uart_rx() {
  uint8_t cur_rx;
  while (uart_is_readable(uart0)) {
    cur_rx = uart_getc(uart0); // Get a char
    if (chars_rxed == 0 && cur_rx == 0xE0) {
      chars_rxed = 1;
    }
    else if (chars_rxed == 1 && cur_rx == 0xC0) { //Start of header
      chars_rxed = 2;
    }
    else if (chars_rxed == 2 && cur_rx == 0x0) {
      chars_rxed = 3;
    }
    else if (chars_rxed == 3 && cur_rx == 0x81) { //End of header
      chars_rxed = 4;
    }
    else if (chars_rxed > 3 && chars_rxed < 39) { //Data payload
        receivedChars[chars_rxed - 4] = cur_rx;
        chars_rxed++;
    }
    else if (chars_rxed == 39 && cur_rx == 0xEA) { //End of data payload
      sendStatusBuffer();
      gpio_put(LED_PIN, !gpio_get(LED_PIN)); //Toggle LED
      chars_rxed = 0; //Reset counter
    }
    else {
      printf("Malformatted Status Message\n");
      chars_rxed = 0; //Reset counter
    }
  }
//  printf("Chars_rxed: %d\n",chars_rxed);
}

//Initialise everything
void initAll() {
  //Initialise I/O
  stdio_usb_init();
  // initialise GPIO (Green LED connected to pin 25)
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  //PIO Init Section
  int offset = pio_add_program(CHANNEL_1_PIO, &sendzpadcommand_program);
  sendzpadcommand_program_init(CHANNEL_1_PIO, CHANNEL_1_SMID, offset, CHANNEL_1_PIN);
  offset = pio_add_program(CHANNEL_2_PIO, &sendzpadcommand_program);
  sendzpadcommand_program_init(CHANNEL_2_PIO, CHANNEL_2_SMID, offset, CHANNEL_2_PIN);
  offset = pio_add_program(CHANNEL_3_PIO, &sendzpadcommand_program);
  sendzpadcommand_program_init(CHANNEL_3_PIO, CHANNEL_3_SMID, offset, CHANNEL_3_PIN);
  offset = pio_add_program(CHANNEL_4_PIO, &sendzpadcommand_program);
  sendzpadcommand_program_init(CHANNEL_4_PIO, CHANNEL_4_SMID, offset, CHANNEL_4_PIN);
  offset = pio_add_program(CHANNEL_5_PIO, &sendzpadcommand_program);
  sendzpadcommand_program_init(CHANNEL_5_PIO, CHANNEL_5_SMID, offset, CHANNEL_5_PIN);
  offset = pio_add_program(CHANNEL_6_PIO, &sendzpadcommand_program);
  sendzpadcommand_program_init(CHANNEL_6_PIO, CHANNEL_6_SMID, offset, CHANNEL_6_PIN);
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


void sentZPadCommand(int channel,int command) {
  command = command << 20; //Shift command to bits 20-26, where bits 27-32 are header (zeros)
  switch (channel) {
    case 1:
      pio_sm_put_blocking(CHANNEL_1_PIO, CHANNEL_1_SMID, command);
    break;
    case 2:
      pio_sm_put_blocking(CHANNEL_2_PIO, CHANNEL_2_SMID, command);
    break;
    case 3:
      pio_sm_put_blocking(CHANNEL_3_PIO, CHANNEL_3_SMID, command);
    break;
    case 4:
      pio_sm_put_blocking(CHANNEL_4_PIO, CHANNEL_4_SMID, command);
    break;
    case 5:
      pio_sm_put_blocking(CHANNEL_5_PIO, CHANNEL_5_SMID, command);
    break;
    case 6:
      pio_sm_put_blocking(CHANNEL_6_PIO, CHANNEL_6_SMID, command);
    break;
    default:
      //printf("Invalid Channel \n");
  }
}

int main() {
  uint8_t in,channel,command;
  initAll();
  //Main Loop
  while(1) {
    in = getchar_timeout_us(10000000); // Get the command from USB serial
    if (in == PICO_ERROR_TIMEOUT) {
      printf("Getchar Timeout\n");
    }
    else if (in == '1') {
      gpio_put(LED_PIN, 1); // Turn on LED (testing)
      //printf("LED ON!\n"); //Test debug serial return
    }
    else if (in == '0') {
      gpio_put(LED_PIN, 0); // Turn on LED (testing)
      //printf("LED OFF!\n");
    }
    else if (in == 'S') { //ZPAD command
      channel = getchar();
      command = getchar();
      sentZPadCommand(channel, command);
    }
  }
}
