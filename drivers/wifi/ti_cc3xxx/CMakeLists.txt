# Copyright (c) 2025-2026 Conclusive Engineering sp. z o. o.
# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources(
  ti_cc3xxx_wifi.c
)

if(CONFIG_WIFI_TI_CC33XX)
  zephyr_library_sources(
    ti_cc33xx_sdk_adapt.c
  )
endif()
