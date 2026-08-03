# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ti_cc140xp_mdr_tx_cca)
target_sources(app PRIVATE
    src/main.c
    src/settings/rcl_settings_wisun.c
    src/wisun/wisun_delta_tables_JP.c
)
