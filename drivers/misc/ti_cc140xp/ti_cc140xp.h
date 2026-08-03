/*
 * Copyright (c) 2026 Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_MISC_TI_CC140XP_H_
#define ZEPHYR_DRIVERS_MISC_TI_CC140XP_H_

#include <zephyr/device.h>
#include <stdint.h>
#include <stdbool.h>

/* Include the TRX host driver headers for type definitions */
#include <trx_host.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief TI CC140xP Transceiver Driver API
 * @defgroup ti_cc140xp_interface TI CC140xP Driver API
 * @ingroup misc_interfaces
 * @{
 */

/**
 * @brief Get the device from device tree
 *
 * @param node_label Device tree node label
 * @return Pointer to device structure, or NULL if not found
 */
#define TI_CC140XP_DT_GET(node_label) DEVICE_DT_GET(DT_NODELABEL(node_label))

/**
 * @brief Initialize and open the TRX Host driver
 *
 * @param dev Pointer to the device structure
 * @param params Parameters for initializing the TRX Host driver
 * @return 0 on success, negative errno on failure
 */
int ti_cc140xp_host_open(const struct device *dev, TRX_Host_Params *params);

/**
 * @brief Close the TRX Host driver
 *
 * @param dev Pointer to the device structure
 */
void ti_cc140xp_host_close(const struct device *dev);

/**
 * @brief Register a stream for reception of data from the TRX
 *
 * @param dev Pointer to the device structure
 * @param id The unique identifier of the stream
 * @param data Pointer to a buffer to store the received data in
 * @param len The max size of data that can fit in the buffer
 * @param callback The callback to execute any time a subscribed event is observed
 * @param subscribed_events The events this stream is subscribed to
 * @return TRX_Host_Status indicating success or failure of registration
 */
TRX_Host_Status ti_cc140xp_register_rx_stream(const struct device *dev,
                                              uint8_t id,
                                              uint8_t *data,
                                              uint16_t len,
                                              TRX_Host_Callback callback,
                                              uint64_t subscribed_events);

/**
 * @brief Store a configuration on the TRX
 *
 * @param dev Pointer to the device structure
 * @param id The unique identifier of the config
 * @param data Pointer to a buffer containing the configuration data
 * @param len The length of the data
 * @param reference The unique reference for this config on the TRX
 * @param force Store the config even if one already exists with same id/reference
 * @param callback The callback to execute any time a subscribed event is observed
 * @param subscribed_events The events this configuration is subscribed to
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_store_config(const struct device *dev,
                                        uint8_t id,
                                        uint8_t *data,
                                        uint16_t len,
                                        uint32_t reference,
                                        bool force,
                                        TRX_Host_Callback callback,
                                        uint64_t subscribed_events);

/**
 * @brief Store a stream on the TRX
 *
 * @param dev Pointer to the device structure
 * @param id The unique identifier of the stream
 * @param data Pointer to a buffer containing the stream data
 * @param len The length of the data
 * @param retention The retention policy for this stream on the TRX
 * @param callback The callback to execute any time a subscribed event is observed
 * @param subscribed_events The events this stream is subscribed to
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_store_stream(const struct device *dev,
                                        uint8_t id,
                                        uint8_t *data,
                                        uint16_t len,
                                        TRX_Stream_Retention retention,
                                        TRX_Host_Callback callback,
                                        uint64_t subscribed_events);

/**
 * @brief Store continuation of a continuous stream on the TRX
 *
 * @param dev Pointer to the device structure
 * @param id The unique identifier of the stream
 * @param data Pointer to a buffer containing the stream data
 * @param len The length of the data
 * @param end_stream True if this is the last chunk of the stream
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_continue_stream(const struct device *dev,
                                           uint8_t id,
                                           uint8_t *data,
                                           uint16_t len,
                                           bool end_stream);

/**
 * @brief Store one or more commands on the TRX
 *
 * @param dev Pointer to the device structure
 * @param cmd_stores Pointer to an array of TRX_Request_CommandStore requests
 * @param num_cmds Number of commands in the cmd_stores array
 * @param cmd_to_submit Optional pointer to the command to submit (NULL if not submitting)
 * @param callback The callback to execute any time a subscribed event is observed
 * @param subscribed_events The events this command is subscribed to
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_store_cmds(const struct device *dev,
                                      TRX_Request_CommandStore *cmd_stores,
                                      uint8_t num_cmds,
                                      TRX_Request_CommandStore *cmd_to_submit,
                                      TRX_Host_Callback callback,
                                      uint64_t subscribed_events);

/**
 * @brief Submit a command to the TRX radio
 *
 * @param dev Pointer to the device structure
 * @param slot Slot the cmd resides in on the TRX
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_submit_cmd(const struct device *dev,
                                      uint8_t slot);

/**
 * @brief Stop a command on the TRX radio
 *
 * @param dev Pointer to the device structure
 * @param slot Slot the cmd resides in on the TRX
 * @param stop_type How command execution shall be stopped
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_stop_cmd(const struct device *dev,
                                    uint8_t slot,
                                    TRX_Command_StopType stop_type);

/**
 * @brief Ping the TRX
 *
 * @param dev Pointer to the device structure
 * @param ping_data Used to customize the TRX response
 * @param callback The callback to execute when ping response is received
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_ping(const struct device *dev,
                                uint8_t ping_data,
                                TRX_Host_Callback callback);

/**
 * @brief Write a configuration to NV on the TRX
 *
 * @param dev Pointer to the device structure
 * @param id The unique identifier of the config
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_persist_config(const struct device *dev,
                                          uint8_t id);

/**
 * @brief Start time synchronization with the TRX
 *
 * @param dev Pointer to the device structure
 * @param callback The callback to execute when time sync events are observed
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_start_time_sync(const struct device *dev,
                                           TRX_Host_Callback callback);

/**
 * @brief Get the current RSSI from the TRX
 *
 * @param dev Pointer to the device structure
 * @param callback The callback to execute when RSSI is retrieved
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_util_get_rssi(const struct device *dev,
                                         TRX_Host_Callback callback);

/**
 * @brief Get the version of firmware on the TRX
 *
 * @param dev Pointer to the device structure
 * @param callback The callback to execute when version is retrieved
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_util_get_version(const struct device *dev,
                                            TRX_Host_Callback callback);

/**
 * @brief Get the MAC UUID from the TRX
 *
 * @param dev Pointer to the device structure
 * @param callback The callback to execute when MAC UUID is retrieved
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_util_get_mac_uuid(const struct device *dev,
                                             TRX_Host_Callback callback);

/**
 * @brief Get the device information of the TRX
 *
 * @param dev Pointer to the device structure
 * @param callback The callback to execute when device info is retrieved
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_util_get_device_info(const struct device *dev,
                                                TRX_Host_Callback callback);

/**
 * @brief Instruct the TRX to enter the SSBL on next reset
 *
 * @param dev Pointer to the device structure
 * @param callback The callback to execute when SSBL entry is confirmed
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_util_enter_ssbl(const struct device *dev,
                                           TRX_Host_Callback callback);

/**
 * @brief Instruct the TRX to enter the requested power mode
 *
 * @param dev Pointer to the device structure
 * @param mode The power mode the TRX shall enter
 * @param callback The callback to execute when power mode is set
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_util_set_power_mode(const struct device *dev,
                                               TRX_PowerMode mode,
                                               TRX_Host_Callback callback);

/**
 * @brief Wake the TRX and block until it is ready to receive commands
 *
 * @param dev Pointer to the device structure
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_util_wake_trx(const struct device *dev);

/**
 * @brief Send a NV erase request to the TRX
 *
 * @param dev Pointer to the device structure
 * @param callback The callback to execute when erase completes
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_erase_nv(const struct device *dev,
                                    TRX_Host_Callback callback);

/**
 * @brief Send a RF mode set request to the TRX
 *
 * @param dev Pointer to the device structure
 * @param rf_mode The RF mode to set
 * @param callback The callback to execute when RF mode is set
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_util_set_rf_mode(const struct device *dev,
                                            TRX_RfMode rf_mode,
                                            TRX_Host_Callback callback);

/**
 * @brief Setup the DIOs on the TRX
 *
 * @param dev Pointer to the device structure
 * @param dio_config Pointer to configurations for the different DIOs available on the TRX
 * @param callback The callback to execute when DIO setup completes
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_util_dio_setup(const struct device *dev,
                                          TRX_DioConfig *dio_config,
                                          TRX_Host_Callback callback);

/**
 * @brief Store a device configuration on the TRX (sets DIO, clock, power mode, RF mode)
 *
 * @param dev Pointer to the device structure
 * @param data Device configuration data
 * @param callback The callback to execute when store completes
 * @return TRX_Host_Status indicating success or failure
 */
TRX_Host_Status ti_cc140xp_store_device_config(const struct device *dev,
                                               TRX_DeviceConfigData data,
                                               TRX_Host_Callback callback);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_MISC_TI_CC140XP_H_ */
