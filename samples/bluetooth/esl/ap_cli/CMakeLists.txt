# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(bt_esl_ap)

# ESL root
set(ESL_ROOT ..)

# ESL SRC
set(ESL_SRC ${ESL_ROOT}/src)

if(KERNEL_VERSION_MAJOR GREATER_EQUAL 4)
    target_compile_definitions(app PRIVATE
    -DZEPHYR_4_0_0_OR_LATER
    -DBT_ESL_AP_HAVE_SUBEVENT_ADJUSTMENT
    -DBT_ESL_DONOT_ENABLE_SET_RANDOM_ADDR_WORKAROUND)
endif()

target_compile_definitions(app PRIVATE
    -DAPPL_ESL_DO_NOT_USE_DEFAULT_CONN_PARAMS)

if(CONFIG_BT_OTS_CLIENT)
  target_compile_definitions(app PRIVATE
      -DAPPL_ESL_AP_OTS_SUPPORT)
endif()

include_directories(
  ${ESL_SRC}/appl
  ${ESL_SRC}/pal
)

target_sources(app PRIVATE
  ${ESL_SRC}/appl/appl_esl_ap.c
  ${ESL_SRC}/appl/appl_main.c
  ${ESL_SRC}/appl/cli_esl_ap.c
  ${ESL_SRC}/bal/BT_esl_pl.c
  ${ESL_SRC}/pal/esl_io.c
)
