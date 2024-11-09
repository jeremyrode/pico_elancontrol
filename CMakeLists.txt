# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NEVER EDIT THE NEXT LINES for Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.0.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(elancontrol C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(elancontrol elancontrol.c )

pico_set_program_name(elancontrol "elancontrol")
pico_set_program_version(elancontrol "0.1")

# Generate PIO header
pico_generate_pio_header(elancontrol ${CMAKE_CURRENT_LIST_DIR}/sendzpadcommand.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(elancontrol 0)
pico_enable_stdio_usb(elancontrol 1)

add_subdirectory(nec_transmit_library)

# Add the standard library to the build
target_link_libraries(elancontrol
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(elancontrol PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
)

# Add any user requested libraries
target_link_libraries(elancontrol
        hardware_pio
        nec_transmit_library
        )

pico_add_extra_outputs(elancontrol)

