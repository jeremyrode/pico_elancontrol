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

add_executable(elancontrol
        elancontrol.c
)

# create map/bin/hex file etc.
pico_add_extra_outputs(elancontrol)

# pull in common dependencies and additional uart hardware support
target_link_libraries(elancontrol pico_stdlib hardware_uart)