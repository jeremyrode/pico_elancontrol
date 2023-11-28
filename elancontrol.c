#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include <sendzpadcommand.pio.h>
//Hardware UART Setup
#define BAUD_RATE 19200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE
// Pin Definitions
#define UART_TX_PIN 12
#define UART_RX_PIN 13
#define LED_PIN 25
#define CHANNEL_1_PIN 17
#define CHANNEL_2_PIN 18
#define CHANNEL_3_PIN 19
#define CHANNEL_4_PIN 20
#define CHANNEL_5_PIN 21
#define CHANNEL_6_PIN 22
// PIO Units
#define CHANNEL_1_PIO pio0
#define CHANNEL_2_PIO pio0
#define CHANNEL_3_PIO pio0
#define CHANNEL_4_PIO pio0
#define CHANNEL_5_PIO pio1
#define CHANNEL_6_PIO pio1
// Offsets, must match with above
#define CHANNEL_1_OFFSET pio0_offset
#define CHANNEL_2_OFFSET pio0_offset
#define CHANNEL_3_OFFSET pio0_offset
#define CHANNEL_4_OFFSET pio0_offset
#define CHANNEL_5_OFFSET pio1_offset
#define CHANNEL_6_OFFSET pio1_offset
// SMIDs
#define CHANNEL_1_SMID 0
#define CHANNEL_2_SMID 1
#define CHANNEL_3_SMID 2
#define CHANNEL_4_SMID 3
#define CHANNEL_5_SMID 0
#define CHANNEL_6_SMID 1
//Send a keepalive even if nothing is changing
//40 bytes in status with 3 bytes as header, 1 in footer (not stored)
#define STATUS_LEN 35
#define KEEPALIVE_INTERVAL 500
#define ZPAD_COMMAND_HEADER 67
#define ELAN_VOLUP  4
#define ELAN_VOLDOWN 36
// Global to keep track of same status
static int identicalStatusRecieved = 0;
static int volume[6];
static bool mute[6];
static int input[6];

// Constants for status data header and footers
const char header[] = {0xE0, 0xC0, 0x00, 0x81};
const char data_footer = 0xEA;
const char err_footer = 0xEF;

// Send error message with header/footer
void sendError(const char *errString) {
  for (int i=0; i < sizeof(header); i++) {
    putchar_raw(header[i]);
  }
  puts_raw(errString);
  putchar_raw(err_footer);
}

void extractData(unsigned char receivedChars[]) {
  //lets send it
  for (int i=0; i < sizeof(header); i++) {
    putchar_raw(header[i]); // Send the header
  }
  for (int i=0; i < STATUS_LEN; i++) {
    putchar_raw(receivedChars[i]); //payload
  }
  putchar_raw(data_footer); //footer to indicate status
  gpio_put(LED_PIN, !gpio_get(LED_PIN)); // Toggle LED on update sent
}

// Hardware UART RX interrupt handler
void on_uart_rx() {
  char cur_rx;
  static int charsRXed = 0;
  static bool identical = true; // Keep track if pattern is identical
  static unsigned char receivedChars[STATUS_LEN];   // an array to store the received data
  static unsigned char lastBuffer[STATUS_LEN];   // an array to store the last received data
  while (uart_is_readable(uart0)) {
    cur_rx = uart_getc(uart0); // Get a char
    if (charsRXed < 4 && cur_rx == header[charsRXed]) { //Header
      charsRXed++;
    }
    else if (charsRXed > 3 && charsRXed < 39) { //Data payload
        receivedChars[charsRXed - 4] = cur_rx;
        charsRXed++;
        if (receivedChars[charsRXed - 4] != lastBuffer[charsRXed - 4]) {
          lastBuffer[charsRXed - 4] = receivedChars[charsRXed - 4]; //Copy
          identical = false; //We are not identical
        }
    }
    else if (charsRXed == 39 && cur_rx == data_footer) { //End of data payload
      if (!identical || identicalStatusRecieved > KEEPALIVE_INTERVAL) {
        extractData(receivedChars); //Send the new status
        identicalStatusRecieved = 0; //Reset counter when status is sent
      }
      else {
        identicalStatusRecieved++; //Counter for keepalive
      }
      charsRXed = 0; //Reset chars recieved counter
      identical = true; //Reset identical flag
    }
    else { //We did not match expected data pattern
      sendError("Malformatted Serial Status Message\n");
      charsRXed = 0; //Reset counter
      identical = true; //Reset identical
    }
  }
}

//Initialise everything
void initAll() {
  //Initialise Standard I/O to USB
  stdio_usb_init();
  // initialise GPIO (Green LED connected to pin 25)
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  //PIO Init Section
  int pio0_offset = pio_add_program(pio0, &sendzpadcommand_program);
  int pio1_offset = pio_add_program(pio1, &sendzpadcommand_program);
  sendzpadcommand_program_init(CHANNEL_1_PIO, CHANNEL_1_SMID, CHANNEL_1_OFFSET, CHANNEL_1_PIN);
  sendzpadcommand_program_init(CHANNEL_2_PIO, CHANNEL_2_SMID, CHANNEL_2_OFFSET, CHANNEL_2_PIN);
  sendzpadcommand_program_init(CHANNEL_3_PIO, CHANNEL_3_SMID, CHANNEL_3_OFFSET, CHANNEL_3_PIN);
  sendzpadcommand_program_init(CHANNEL_4_PIO, CHANNEL_4_SMID, CHANNEL_4_OFFSET, CHANNEL_4_PIN);
  sendzpadcommand_program_init(CHANNEL_5_PIO, CHANNEL_5_SMID, CHANNEL_5_OFFSET, CHANNEL_5_PIN);
  sendzpadcommand_program_init(CHANNEL_6_PIO, CHANNEL_6_SMID, CHANNEL_6_OFFSET, CHANNEL_6_PIN);
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

void sentZPadCommand(int channel, int command) {
  if (command < 0 || command > 63) {
    sendError("Invalid Command\n");
    return;
  }
  command = command << 21; //Shift command to upper bits with 6 zero header
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
      sendError("Invalid Channel\n");
  }
}

int main() {
  int in,channel,command;
  char errmsg[50];
  initAll();
  //Main Loop
  while(1) {
    in = getchar_timeout_us(10000000); // Get the command from USB serial
    if (in == PICO_ERROR_TIMEOUT) {
      identicalStatusRecieved = KEEPALIVE_INTERVAL; //Prime us for rapid start
      continue;
    }
    else if (in == ZPAD_COMMAND_HEADER) { //ZPAD command
      channel = getchar_timeout_us(10000);
      if (channel == PICO_ERROR_TIMEOUT) {
        sendError("Channel Timeout\n");
        continue;
      }
      command = getchar_timeout_us(10000);
      if (command == PICO_ERROR_TIMEOUT) {
        sendError("Command Timeout\n");
        continue;
      }
      sentZPadCommand(channel, command);
      if (command == ELAN_VOLUP || command == ELAN_VOLDOWN) {
        sentZPadCommand(channel, command); // Volume commands should be 2X
      }
    }
    else {
      sprintf(errmsg,"Invalid Command: %d\n",in);
      sendError(errmsg);
    }
  }
}
