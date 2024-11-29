# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.12)

# Pull in PICO SDK (must be before project)
include(pico_sdk_import.cmake)
add_subdirectory(pico-ads1115/lib ads1115)

project(pico)

# Initialize the SDK
pico_sdk_init()

add_subdirectory(FatFs_SPI build)
include(example_auto_set_url.cmake)

add_executable(pico
    main.cpp
    misc.cpp
    lora.cpp
    cutdown.cpp
    helpers/repeater.cpp
    helpers/memory.cpp
    helpers/sd.cpp
    helpers/sd_hw_config.cpp
    sensors/bme.cpp
    sensors/gps.cpp
    sensors/no2.cpp
    sensors/muon.cpp
    sensors/solar.cpp
    sensors/pm.cpp
    sensors/tmp117.cpp
    sensors/aht20.cpp
)

target_link_libraries(pico 
    hardware_watchdog
    hardware_spi 
    hardware_i2c 
    hardware_adc
    hardware_flash
    pico_stdlib 
    pico-ads1115
    pico_multicore
    pico_sync
    pico_time
    FatFs_SPI
)

pico_enable_stdio_usb(pico 1)
pico_enable_stdio_uart(pico 0)

pico_add_extra_outputs(
    pico
)