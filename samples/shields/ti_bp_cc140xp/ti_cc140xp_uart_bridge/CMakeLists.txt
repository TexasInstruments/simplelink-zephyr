# Wireless UART Bridge - Zephyr Application
# For CC2745 LaunchPad + CC140xP BoosterPack

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ti_cc140xp_uart_bridge)

# Include TI CC140xP driver headers and local settings
target_include_directories(app PRIVATE
    ${ZEPHYR_BASE}/drivers/misc/ti_cc140xp/host
    ${ZEPHYR_BASE}/drivers/misc/ti_cc140xp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/settings
)

# Application sources
target_sources(app PRIVATE
    src/main.c
    src/settings/rcl_settings_wisun.c
)

# The TI CC140xP driver is linked automatically via CONFIG_TI_CC140XP=y in prj.conf