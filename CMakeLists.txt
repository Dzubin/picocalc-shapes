# PicoCalc Shapes  (by Thomas Dzubin)
#
# Reuses the drivers from ./picocalc-text-starter-main (lcd, southbridge),
# which is picocalc-text-starter by Blair Leduc, plus the Raspberry Pi Pico SDK.
# Build it the same way you build that starter: with the Raspberry Pi Pico VS
# Code extension, or from a terminal that has PICO_SDK_PATH set.

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
set(sdkVersion 2.3.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in the Pico SDK (must be before project())
include(pico_sdk_import.cmake)

project(picocalc-shapes C CXX ASM)

pico_sdk_init()

# Location of the picocalc-text-starter drivers we build against
set(STARTER ${CMAKE_CURRENT_LIST_DIR}/picocalc-text-starter-main)

add_executable(picocalc-shapes
    picocalc-shapes.c
    ${STARTER}/drivers/lcd.c
    ${STARTER}/drivers/southbridge.c
    ${STARTER}/drivers/font-8x10.c
    ${STARTER}/drivers/font-5x10.c
)

pico_set_program_name(picocalc-shapes "picocalc-shapes")
pico_set_program_version(picocalc-shapes "0.1")

# No serial console needed; keep USB free so '~' -> BOOTSEL is clean
pico_enable_stdio_uart(picocalc-shapes 0)
pico_enable_stdio_usb(picocalc-shapes 0)

target_compile_options(picocalc-shapes PRIVATE -Wall)

target_include_directories(picocalc-shapes PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${STARTER}
    ${STARTER}/drivers
)

target_link_libraries(picocalc-shapes
    pico_stdlib
    hardware_gpio
    hardware_i2c
    hardware_spi
    hardware_watchdog   # watchdog_reboot(): leaving for the UF2 Loader
)

# The chip is part of every output file name (shapes-RP2040.uf2 /
# shapes-RP2350.uf2, and the matching .elf/.bin/.hex), so a downloaded file
# always says which board it was built for.
if(PICO_PLATFORM MATCHES "^rp2040")
    set(CHIP_NAME RP2040)
elseif(PICO_PLATFORM MATCHES "^rp2350")
    set(CHIP_NAME RP2350)
else()
    set(CHIP_NAME ${PICO_PLATFORM})
endif()
set_target_properties(picocalc-shapes PROPERTIES OUTPUT_NAME shapes-${CHIP_NAME})

pico_add_extra_outputs(picocalc-shapes)

# Copy the finished .uf2 (same name) to the top-level project folder, so the
# RP2040 and RP2350 builds can sit side by side.
add_custom_command(TARGET picocalc-shapes POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE_DIR:picocalc-shapes>/$<TARGET_FILE_BASE_NAME:picocalc-shapes>.uf2
            ${CMAKE_CURRENT_LIST_DIR}/
)

