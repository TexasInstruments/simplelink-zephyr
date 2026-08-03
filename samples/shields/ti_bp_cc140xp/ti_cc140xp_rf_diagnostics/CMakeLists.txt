# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ti_cc140xp_rf_diagnostics)

add_compile_definitions(DUAL_RADIOS TRX_CC1407)

target_include_directories(app PRIVATE src)

target_sources(app PRIVATE
    src/main.c
    src/at/AtTerm.c
    src/at/AtProcess.c
    src/at/AtControl.c
    src/at/AtParams.c
    src/TestMode.c
    src/radio/radio_common.c
    src/radio/radio_builtin_stub.c
    src/radio/trx/radio_trx.c
    src/radio/trx/radio_trx_mdr.c
    src/radio/trx/PhySettings_trx.c
    src/radio/trx/PhySettings_trx_mdr.c
    src/settings/rcl_settings_2gfsk38p4kbps.c
    src/settings/rcl_settings_2gfsk50kbps.c
    src/settings/rcl_settings_2gfsk50kbps_lp.c
    src/settings/rcl_settings_2gfsk9p6kbps.c
    src/settings/rcl_settings_dualrx_sun_oqpsk_2gfsk38p4kbps.c
    src/settings/rcl_settings_general_purpose_gfsk.c
    src/settings/rcl_settings_ofdmlr_16p67ksps.c
    src/settings/rcl_settings_ofdmlr_66p67ksps.c
    src/settings/rcl_settings_ofdmlr_8p33ksps.c
    src/settings/rcl_settings_sunoqpsk.c
    src/settings/rcl_settings_wisun.c
    src/settings/rcl_settings_xlr_20ksps.c
    src/wisun/wisun_delta_tables_EU1.c
    src/wisun/wisun_delta_tables_JP.c
    src/wisun/wisun_delta_tables_NA.c
)
