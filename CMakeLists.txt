# Set minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Include build functions from Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Set name of project (as PROJECT_NAME) and C/C   standards
project(pico_elancontrol C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Creates a pico-sdk subdirectory in our project for the libraries
pico_sdk_init()

# Defines the intent to create an executable file (.bin, .uf2 etc.),
# the first argument is the name, the second argument are the source files.
add_executable(elancontrol elancontrol.c)

# create map/bin/hex file etc.
pico_add_extra_outputs(elancontrol)

# pull in common dependencies and additional uart hardware support
# if UART hardware_uart
# add PIO here if needed hardware_pio
# Others: hardware_flash pico_multicore pico_stdlib tinyusb_device
target_link_libraries(elancontrol pico_stdlib hardware_pio)

# If and when PIO used
pico_generate_pio_header(elancontrol ${CMAKE_CURRENT_LIST_DIR}/sendzpadcommand.pio)

# Control UARTs to STDIO
pico_enable_stdio_usb(elancontrol 1)
#pico_enable_stdio_uart(elancontrol 1)
