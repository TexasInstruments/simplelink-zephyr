# Copyright (c) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: Apache-2.0

zephyr_library()

# Main driver source
zephyr_library_sources_ifdef(CONFIG_TI_CC140XP ti_cc140xp.c)

# Host library sources
zephyr_library_sources_ifdef(CONFIG_TI_CC140XP
    host/trx_host.c
    host/handlers/trx_host_handlers.c
    host/private/trx_host_private.c
    host/transport/spi_transport.c
    host/utility/utils.c
    host/utility/list/list.c
    host/utility/buffer_manager/buffer_manager.c
    host/hal/platform_util.c
    host/hal/platform_spi.c
)

# Include directories for host library and make them public
zephyr_library_include_directories(host)
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/host)

# Make the driver header publicly available to applications
zephyr_include_directories(${ZEPHYR_BASE}/drivers/misc)
