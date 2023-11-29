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
#define KEEPALIVE_INTERVAL 50
#define ZPAD_COMMAND_HEADER 67
#define SLIDER_COMMAND_HEADER 68
#define ELAN_VOLUP  4
#define ELAN_VOLDOWN 36
// Global to keep track of same status
static int identicalStatusRecieved = 0;
static int sliderTargetVol[6] = {-1,-1,-1,-1,-1,-1};
static struct statusStruct curStatus;
// Constants for status data header and footers
const char header[] = {0xE0, 0xC0, 0x00, 0x81};
const char data_footer = 0xEA;
const char err_footer = 0xEF;
const int zoneToChannel[] = {5, 1, 4, 2, 6, 3}; // Zone to channel (depends on how plugs are ordered)
//Unpacked status from serial
struct statusStruct {
  int volume[6];
  int input[6];
  bool mute[6];
};

// Send error message with header/footer
void sendError(const char *errString) {
  for (int i=0; i < sizeof(header); i++) {
    putchar_raw(header[i]);
  }
  puts_raw(errString);
  putchar_raw(err_footer);
  fflush(stdout);
}
//Unpack the data1
struct statusStruct unPackData(unsigned char receivedChars[]) {
  struct statusStruct status;
  for (int i=0; i < 6; i++) {
    status.volume[i] = 48 - receivedChars[i*6+2] & 0b00111111; //Volume is sent as a 6-bit attenuation value in LSBs
    status.mute[i] = (receivedChars[i*6] & 0b00010000) >> 4; // Mute is just a bit
    status.input[i] = receivedChars[i*6] & 0b00000111; // Status is 3 LSBs
  }
  return status;
}
//Compair Status structures
bool isStatusDiff(struct statusStruct data1, struct statusStruct data2) {
  bool is_diff = false;
  for (int i=0; i < 6; i++) {
      is_diff = is_diff || (data1.volume[i] != data2.volume[i]);
      is_diff = is_diff || (data1.mute[i] != data2.mute[i]);
      is_diff = is_diff || (data1.input[i] != data2.input[i]);
  }
  return true;
}
//Send commands to PIO state machines
void sentZPadCommand(int channel, int command) {
  if (command < 0 || command > 63) {
    sendError("Invalid Command");
    return;
  }
  command = command << 21; //Shift command to upper bits with 6 zeroa as header
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
      sendError("Invalid Channel");
  }
}
//Handle slider updates
void handleSliderUpdates () {
  for (int i; i < 6; i++) {
    if (sliderTargetVol[i] > 0) {
      if (sliderTargetVol[i] > curStatus.volume[i]) {
        sentZPadCommand(i, ELAN_VOLUP);
        sentZPadCommand(i, ELAN_VOLUP);
      }
      else if (sliderTargetVol[i] < curStatus.volume[i]) {
        sentZPadCommand(i, ELAN_VOLDOWN);
        sentZPadCommand(i, ELAN_VOLDOWN);
      }
      else {
        sliderTargetVol[i] = -1; //We're done
      }
    }
  }
}
//Send a status with header/footer
void sendData(struct statusStruct status) {
  for (int i=0; i < sizeof(header); i++) {
    putchar_raw(header[i]); // Send the header
  }
  for (int i=0; i < 6; i++) {
    putchar_raw(status.volume[i]);
    putchar_raw(status.mute[i]);
    putchar_raw(status.input[i]);
  }
  putchar_raw(data_footer); //footer to indicate this is a status
  fflush(stdout);
  identicalStatusRecieved = 0; //Reset counter when status is sent
  gpio_put(LED_PIN, !gpio_get(LED_PIN)); // Toggle LED on update sent
}
//Handle data
void handleRXData(unsigned char receivedChars[]) {
  struct statusStruct newStatus;
  newStatus = unPackData(receivedChars);
  if (isStatusDiff(newStatus,curStatus) || identicalStatusRecieved > KEEPALIVE_INTERVAL) {
    sendData(newStatus);
    curStatus = newStatus;
    handleSliderUpdates(); //Do we care if this happends on timeouts?
  }
}
// Hardware UART RX interrupt handler
void on_uart_rx() {
  char cur_rx;
  //Statics to keep track between interrupts
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
      if (identical && identicalStatusRecieved <= KEEPALIVE_INTERVAL) {
        identicalStatusRecieved++; //Counter for keepalive
      }
      else {
        handleRXData(receivedChars); //Send the new status
      }
      charsRXed = 0; //Reset chars recieved counter
      identical = true; //Reset identical flag
    }
    else { //We did not match expected data pattern
      sendError("Malformatted Serial Status Message");
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

//Handle a request to send a straight ZPAD command
void handleZPADCommand() {
  int channel, command;
  if ((channel = getchar_timeout_us(10000)) == PICO_ERROR_TIMEOUT) {
    sendError("Channel Timeout");
    return;
  }
  if ((command = getchar_timeout_us(10000)) == PICO_ERROR_TIMEOUT) {
    sendError("Command Timeout");
    return;
  }
  sentZPadCommand(channel, command);
  if (command == ELAN_VOLUP || command == ELAN_VOLDOWN) {
    sentZPadCommand(channel, command); // Volume commands should be doubled
  }
}

//Handle a slider request
void handleSliderReq() {
  int zone, volume, channel;
    char errstr[50];
  if ((zone = getchar_timeout_us(10000)) == PICO_ERROR_TIMEOUT) {
    sendError("Zone Timeout");
    return;
  }
  if ((volume = getchar_timeout_us(10000)) == PICO_ERROR_TIMEOUT) {
    sendError("Volume Timeout");
    return;
  }
  channel = zoneToChannel[zone];
  sliderTargetVol[channel] = volume; //set the target
  if (volume < curStatus.volume[channel]) {
    sentZPadCommand(channel, ELAN_VOLUP);
    sentZPadCommand(channel, ELAN_VOLUP);
  }
  else {
    sentZPadCommand(channel, ELAN_VOLDOWN);
    sentZPadCommand(channel, ELAN_VOLDOWN);
  }
  sprintf(errstr,"Got a Slider Request, Channel: %d, Vol: %d", channel, volume);
  sendError(errstr);
}
//Main: init, then infinite loop to handle commands
int main() {
  int in;
  initAll();
  //Main Loop
  while(1) {
    in = getchar_timeout_us(10000000); // Get the command from USB serial
    switch (in) {
      case PICO_ERROR_TIMEOUT:
        identicalStatusRecieved = KEEPALIVE_INTERVAL; //Prime us for rapid start
      break;
      case ZPAD_COMMAND_HEADER:
        handleZPADCommand();
      break;
      case SLIDER_COMMAND_HEADER:
        handleSliderReq();
      break;
      default:
      sendError("Invalid Command");
    }
  }
}
