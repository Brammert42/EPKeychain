cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(btckeychain C CXX ASM)

pico_sdk_init()

add_executable(btckeychain
    src/main.cpp
    src/bip32.cpp
    src/btcaddr.cpp
    src/epd_qr.cpp
    src/config_fs.cpp
    src/qrcodegen.c
    src/usb_descriptors.c
    waveshare_epd/epd1in54_V2.cpp
    waveshare_epd/epdif.cpp
    waveshare_epd/epdpaint.cpp
    waveshare_epd/font8.c
    waveshare_epd/font12.c
    waveshare_epd/font16.c
    waveshare_epd/font20.c
    waveshare_epd/font24.c
)

target_include_directories(btckeychain PRIVATE
    src
    waveshare_epd
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(btckeychain
    pico_stdlib
    hardware_spi
    hardware_flash
    hardware_sync
    tinyusb_device
    tinyusb_board
)

pico_add_extra_outputs(btckeychain)
