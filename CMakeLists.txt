cmake_minimum_required(VERSION 3.13)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

project(usb_screen C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.5.1")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.5.1 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()


# Initialize the SDK
pico_sdk_init()

# Add executable
add_executable(usb_screen
        ${CMAKE_CURRENT_LIST_DIR}/main.c
        ${CMAKE_CURRENT_LIST_DIR}/lcd.c
        ${CMAKE_CURRENT_LIST_DIR}/disk.c
        ${CMAKE_CURRENT_LIST_DIR}/fat12.c
        ${CMAKE_CURRENT_LIST_DIR}/bmp.c
        ${CMAKE_CURRENT_LIST_DIR}/usb_drive.c
        ${CMAKE_CURRENT_LIST_DIR}/button.c
        )

# Include freertos
set(PICO_FREERTOS_PATH $ENV{PICO_FREERTOS_PATH})
include(${PICO_FREERTOS_PATH}/CMakeLists.txt)

# Settings for TinyUSB
set(FAMILY rp2040)
set(BOARD pico_sdk)
set(TINYUSB_FAMILY_PROJECT_NAME_PREFIX "tinyusb_dev_")
add_subdirectory(${PICO_TINYUSB_PATH}/examples/device tinyusb_device_examples)

# Extra includes
target_include_directories(usb_screen PUBLIC
        ${CMAKE_CURRENT_LIST_DIR})

# enable usb output, disable uart output
pico_enable_stdio_usb(usb_screen 0)
pico_enable_stdio_uart(usb_screen 1)

# pull in common dependencies
target_link_libraries(usb_screen PUBLIC
        pico_stdlib 
        pico_unique_id 
        hardware_spi
        freertos
        tinyusb_device 
        tinyusb_board
        )

# create map/bin/hex file etc.
pico_add_extra_outputs(usb_screen)

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        -Wno-maybe-uninitialized
        )

add_compile_definitions(configENABLE_BACKWARD_COMPATIBILITY=1)