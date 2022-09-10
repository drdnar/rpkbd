cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(rpkbd C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(rpkbd
    rpkbd.c
)

set(PICO_COPY_TO_RAM 1)
#set(PICO_NO_FLASH 1)
# or could do pico_set_binary_type(rpkbd copy_to_ram)
# or could do pico_set_binary_type(rpkbd no_flash)
set(PICO_BOARD pimoroni_pga2040) # Defaults to Pico

target_sources(rpkbd PUBLIC
    usb_descriptors.c
    rpkbd.c
    keyboard_matrix.c)

# Make sure TinyUSB can find tusb_config.h
target_include_directories(rpkbd PUBLIC
        ${CMAKE_CURRENT_LIST_DIR})

target_compile_definitions(rpkbd PRIVATE
    PICO_DEFAULT_UART_TX_PIN=28
    PICO_DEFAULT_UART_RX_PIN=29
)

pico_enable_stdio_uart(rpkbd 1)

pico_add_extra_outputs(rpkbd)

target_link_libraries(rpkbd pico_stdlib tinyusb_device tinyusb_board)

target_compile_definitions(rpkbd PUBLIC PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1)

