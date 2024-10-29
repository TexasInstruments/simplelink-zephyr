# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_link_libraries(subsys__bluetooth)

zephyr_include_directories(
  .
  inc
  icall/inc
  icall/src/inc
  ll/inc
  osal/src/inc
  rom
  hal
  hal/inc
  hci
  common
)


zephyr_library_sources(
  #common
  common/onboard.c
  #hal
  hal/src/hal_assert.c
  # LL
  ll/src/ble.c
  ll/src/ll_init.c
  ll/src/ll_common.c
  ll/src/ll_access_address.c
  ll/src/ll_adv_end_causes.c
  ll/src/ll_scan_end_causes.c
  ll/src/ll_init_end_causes.c
  ll/src/ll_central_end_causes.c
  ll/src/ll_peripheral_end_causes.c
  ll/src/ll_enc.c
  ll/src/ll.c
  ll/src/ll_isr.c
  ll/src/ll_timer_drift.c
  ll/src/ll_config.c
  ll/src/ll_scheduler.c
  ll/src/ll_rat.c
  ll/src/ll_ae.c
  ll/src/ll_ecc.c
  ll/src/ll_al.c
  ll/src/ll_privacy.c
  ll/src/ll_data.c
  ll/src/ll_dfl.c
  # OSAL  
  osal/src/common/osal.c
  osal/src/common/osal_timers.c
  osal/src/common/osal_cbtimer.c
  osal/src/common/osal_bufmgr.c
  osal/src/common/osal_pwrmgr.c
  osal/src/common/osal_clock.c
  osal/src/common/osal_memory_icall.c
  # HCI
  hci/hci_data.c
  hci/hci_event_raw.c
  hci/hci_tl.c
  hci/hci.c
  # ICall
  icall/src/icall_POSIX.c
  icall/src/icall_user_config.c
  icall/src/icall_lite_translation.c
  icall/stack/ble_dispatch_lite.c
  icall/stack/ble_user_config_stack.c
  icall/app/icall_api_lite.c
  icall/app/icall_hci_tl.c

  icall/app/ble_user_config.c
  
  # ROM 
  rom/rom_init.c

  # Zephyr
)

set(SCAN_REQ_RPT_CFG  0x02)

set(PHY_2MBPS_CFG     0x01)
set(PHY_LR_CFG        0x02)
set(HDC_NC_ADV_CFG    0x04)
set(AE_CFG            0x08)
set(CHAN_ALGO2_CFG    0x40)

set(ADV_NCONN_CFG     0x01)
set(ADV_CONN_CFG      0x02)
set(SCAN_CFG          0x04)
set(INIT_CFG          0x08)
set(CTRL_CONFIG       0)

set(BLE_VS_FEATURES   SCAN_REQ_RPT_CFG)
set(BLE_V50_FEATURES  PHY_2MBPS_CFG+PHY_LR_CFG+HDC_NC_ADV_CFG+CHAN_ALGO2_CFG+AE_CFG)


if (CONFIG_BT_HCI_RAW)
  set(CTRL_CONFIG       CTRL_CONFIG+ADV_NCONN_CFG+ADV_CONN_CFG+SCAN_CFG+INIT_CFG)
endif() # BT_HCI_RAW


if (CONFIG_BT_BROADCASTER)
  set(CTRL_CONFIG       CTRL_CONFIG+ADV_NCONN_CFG+ADV_CONN_CFG)
endif() # CONFIG_BT_BROADCASTER

if (CONFIG_BT_EXT_ADV)
  set(CTRL_CONFIG       CTRL_CONFIG+ADV_NCONN_CFG)
  zephyr_compile_definitions(
    -DUSE_AE
  )  
endif() # CONFIG_BT_EXT_ADV

if (CONFIG_BT_PER_ADV)
  set(CTRL_CONFIG       CTRL_CONFIG+ADV_NCONN_CFG)
  zephyr_compile_definitions(
    -DUSE_AE
    -DUSE_PERIODIC_ADV
  )

endif() # CONFIG_BT_PER_ADV

if (CONFIG_BT_OBSERVER)
  set(CTRL_CONFIG       CTRL_CONFIG+SCAN_CFG)
endif() # CONFIG_BT_OBSERVER

if (CONFIG_BT_PERIPHERAL)
  set(CTRL_CONFIG       CTRL_CONFIG+ADV_NCONN_CFG+ADV_CONN_CFG)
  zephyr_compile_definitions(
    -DUSE_AE
  )
endif() # CONFIG_BT_PERIPHERAL

if (CONFIG_BT_CENTRAL)
  set(CTRL_CONFIG       CTRL_CONFIG+SCAN_CFG+INIT_CFG)
endif() # CONFIG_BT_CENTRAL

zephyr_compile_definitions(
-DCC23X0
-DCONTROLLER_ONLY

-DQUAL_TEST
-DDISABLE_AUTO_FEATURE_REQ
-DDISABLE_VS_EVENTS

-DLEGACY_CMD

-DDisplay_DISABLE_ALL
-DICALL_EVENTS
-DUSE_ICALL
-DICALL_JT
-DICALL_LITE
-DICALL_MAX_NUM_ENTITIES=6
-DICALL_MAX_NUM_TASKS=3
-DICALL_STACK0_ADDR
-DMAX_NUM_BLE_CONNS=8
-DMAX_PDU_SIZE=255
-DxPOWER_SAVING
-DSTACK_LIBRARY
-DOSAL_CBTIMER_NUM_TASKS=1
-DxUSE_RCOSC
-DONE_BLE_LIB_SIZE_OPTIMIZATION

-DHCI_TL_FULL
-DICALL_LITE_12_PARAMS
-DNO_OSAL_SNV


-DADV_NCONN_CFG
-DADV_CONN_CFG
-DSCAN_CFG
-DINIT_CFG
-DUSE_AE
-DUSE_PERIODIC_ADV
-DCTRL_CONFIG   
-DBLE_VS_FEATURES  
-DBLE_V50_FEATURES 
-DEXT_HAL_ASSERT
)
