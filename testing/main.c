#include <stdio.h>
#include "pico/stdlib.h"
#include <rxSerial.pio.h>

#define UART_RX_PIN 13
#define LED_PIN 25

//Initialise everything
void initAll() {
  //Initialise I/O
  stdio_usb_init();
  // initialise GPIO (Green LED connected to pin 25)
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  //PIO Init Section
  int pio0_offset = pio_add_program(pio0, &rxSerial_program);
  rxSerial_program_init(pio0, 0, pio0_offset, UART_RX_PIN);
}

int main() {
  static uint32_t out;
  initAll();
  while (1) {
    out = pio_sm_get_blocking(pio0,0);
    putchar_raw(out);
  }
}
