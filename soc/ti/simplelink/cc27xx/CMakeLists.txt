# Copyright (c) 2025 Texas Instruments Incorporated
# Copyright (c) 2024 BayLibre, SAS
#
# SPDX-License-Identifier: Apache-2.0

zephyr_sources(soc.c)
zephyr_sources(ccfg.c)
zephyr_sources(scfg.c)
zephyr_sources(power.c)
zephyr_include_directories(.)

zephyr_linker_sources_ifdef(CONFIG_HAS_TI_CCFG SECTIONS ccfg.ld)

zephyr_linker_sources_ifdef(CONFIG_HAS_TI_SCFG SECTIONS scfg.ld)

set_property(GLOBAL APPEND PROPERTY extra_post_build_commands
  # Insert CRC32 in CCFG
  COMMAND crc_tool patch-image
	  --elf ${CMAKE_BINARY_DIR}/zephyr/${KERNEL_ELF_NAME}
	  --symbol-prefix CRC_CCFG
	  --output ${CMAKE_BINARY_DIR}/zephyr/${KERNEL_ELF_NAME}
  COMMAND ${ZEPHYR_SDK_INSTALL_DIR}/arm-zephyr-eabi/bin/arm-zephyr-eabi-objcopy -O ihex ${CMAKE_BINARY_DIR}/zephyr/${KERNEL_ELF_NAME} ${CMAKE_BINARY_DIR}/zephyr/${KERNEL_HEX_NAME}

  # Insert CRC32 in SCFG
  COMMAND crc_tool patch-image
	  --elf ${CMAKE_BINARY_DIR}/zephyr/${KERNEL_ELF_NAME}
	  --symbol-prefix CRC_SCFG
	  --output ${CMAKE_BINARY_DIR}/zephyr/${KERNEL_ELF_NAME}
  COMMAND ${ZEPHYR_SDK_INSTALL_DIR}/arm-zephyr-eabi/bin/arm-zephyr-eabi-objcopy -O ihex ${CMAKE_BINARY_DIR}/zephyr/${KERNEL_ELF_NAME} ${CMAKE_BINARY_DIR}/zephyr/${KERNEL_HEX_NAME}
  )

set(SOC_LINKER_SCRIPT ${ZEPHYR_BASE}/include/zephyr/arch/arm/cortex_m/scripts/linker.ld CACHE INTERNAL "")
