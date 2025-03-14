# Set minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Include build functions from Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Set name of project (as PROJECT_NAME) and C/C   standards
project(pico_jr_testing C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# Defines the intent to create an executable file (.bin, .uf2 etc.),
# the first argument is the name, the second argument are the source files.
add_executable(pico_jr_testing main.c)

# create map/bin/hex file etc.
pico_add_extra_outputs(pico_jr_testing)

#pull in common dependencies and additional uart hardware support
# if UART hardware_uart
# add PIO here if needed hardware_pio
# Others: hardware_flash pico_multicore pico_stdlib tinyusb_device hardware_pio

target_link_libraries(pico_jr_testing pico_stdlib)

# If and when PIO used
#pico_generate_pio_header(pico_jr_testing ${CMAKE_CURRENT_LIST_DIR}/rxSerial.pio)

# Control UARTs to STDIO
pico_enable_stdio_usb(pico_jr_testing 1)
#pico_enable_stdio_uart(pico_jr_testing 1)
