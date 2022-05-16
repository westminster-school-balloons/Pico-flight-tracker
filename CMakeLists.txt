cmake_minimum_required(VERSION 3.12)

# Pull in PICO SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico)

# Initialize the SDK
pico_sdk_init()

include(example_auto_set_url.cmake)

add_executable(pico
    main.cpp
    misc.cpp
    lora.cpp
    cutdown.cpp
    helpers/repeater.cpp
    helpers/memory.cpp
    sensors/bme.cpp
    sensors/gps.cpp
    sensors/no2.cpp
    sensors/muon.cpp
)

target_link_libraries(pico 
    hardware_watchdog
    hardware_spi 
    hardware_i2c 
    hardware_adc
    hardware_flash
    pico_stdlib 
    pico_multicore
    pico_sync
    pico_time
)

pico_enable_stdio_usb(pico 0)
pico_enable_stdio_uart(pico 1)

pico_add_extra_outputs(
    pico
)