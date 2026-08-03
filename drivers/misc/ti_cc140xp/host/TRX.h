/*
 * Copyright (c) 2022-2026 Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 *  @file ti/trx/TRX.h
 *  @brief Common API file for TRX.
 *  @addtogroup ti_trx_Trx
 */

#ifndef ti_trx_TRX__h
#define ti_trx_TRX__h
#include <stdint.h>

/** @defgroup ti_trx_Trx TRX Wire Interface
 *  External headers for communication with TRX device
 *  @{
 */

/**
 * @brief Size of an SPI block.
 */
#define TRX_SPIBLOCK_SIZE 128

typedef union TRX_TransactionHeader {
    struct {
        uint32_t free       : 4; /*!< (0-14) Number of free SPI blocks currently available. (15) more than 14 blocks available */
        uint32_t res0       : 1; /*!< Reserved */
        uint32_t error      : 1; /*!< Error state detected, poll device until error message is read out */
        uint32_t data_rdy_n : 1; /*!< Whether this transaction has payload (0) or is empty (1) */
        uint32_t chip_rdy_n : 1; /*!< MSB: Whether device is ready (0) or not (1) */
        uint32_t len        : 8; /*!< Length of the transaction payload */
        uint32_t tid_tx     : 4; /*!< Transaction identifier of this transaction */
        uint32_t tid_ack    : 4; /*!< Transaction identifier of acknowledged transaction */
        uint32_t status     : 8; /*!< Device status */
    } trx;                       /*!< Transaction header for TRX -> Host direction */
    struct {
        uint32_t strobe       : 8; /*!< Strobe command */
        uint32_t len          : 8; /*!< Length of the transaction payload */
        uint32_t tid_tx       : 4; /*!< Transaction identifier of this transaction */
        uint32_t tid_ack      : 4; /*!< Transaction identifier of acknowledged transaction */
        uint32_t strobe_param : 8; /*!< Parameter provided to the strobe command */
    } host;                        /*!< Transaction header for Host -> TRX direction */
    uint32_t u32;
    uint8_t  u8[4];
} TRX_TransactionHeader;

typedef struct TRX_TransactionFooter {
    uint16_t crc; /*!< CRC calculated over header and payload */
} TRX_TransactionFooter;

/**
 * @brief Enumeration of strobe commands usable in the
 *     ::TRX_TransactionHeader.host &rarr; strobe field.
 */
typedef enum TRX_StrobeCommand {
    TRX_StrobeCommand_None     = 0,
    TRX_StrobeCommand_Magic    = 0x7E,
    TRX_StrobeCommand_Reserved = 0x7F,
} TRX_StrobeCommand;

/**
 * @brief Available options for the MAGIC strobe command.
 */
typedef enum TRX_StrobeParam_Magic_e {
    TRX_StrobeParam_Magic_SWD_IO_LRF   = 0x1,
    TRX_StrobeParam_Magic_SWD_IO_RESET = 0x2,
    TRX_StrobeParam_Magic_DIO0_GPI     = 0x3,
    TRX_StrobeParam_Magic_DIO0_SFD     = 0x4,
    TRX_StrobeParam_Magic_DIO1_GPI     = 0x5,
    TRX_StrobeParam_Magic_DIO1_TRC     = 0x6,
    TRX_StrobeParam_Magic_CC1407_DIO0_GPI = 0x3,
    TRX_StrobeParam_Magic_CC1407_DIO0_SFD = 0x4,
    TRX_StrobeParam_Magic_CC1407_DIO4_GPI = 0x5,
    TRX_StrobeParam_Magic_CC1407_DIO4_TRC = 0x6,
} TRX_StrobeParam_Magic;

/**
 * @brief Union of param values depending on command type.
 */
typedef union TRX_StrobeParam_u {
    TRX_StrobeParam_Magic magic;
    uint32_t              u32;
} TRX_StrobeParam;

/**
 * @brief Type for buffer holding an SPI block.
 *
 */
typedef struct {
    TRX_TransactionHeader header;    /*!< Union for transaction headers in both directions */
    uint8_t               payload[]; /*!< Transaction payload */
} TRX_SpiBlock;

/**
 * @brief Requests types that are recognized by the TRX device
 *
 */
typedef enum RequestType_e {
    Req_INVALID               = 0,   /*!< Invalid request ID */
    Req_LAST_STATUS           = 1,   /*!< Last status, see ::TRX_Request_LastStatus */
    Req_TRANSPORT_ERROR       = 2,   /*!< Transport error, see ::TRX_Request_TransportError */
    Req_CMD_STORE             = 16,  /*!< Store a radio command, see ::TRX_Request_CommandStore */
    Req_CMD_SUBMIT            = 17,  /*!< Submit a radio command, see ::TRX_Request_CommandSubmit */
    Req_CMD_STOP              = 18,  /*!< Stop a radio command, see ::TRX_Request_CommandStop */
    Req_CMD_STATUS            = 19,  /*!< Command completion status, see ::TRX_Request_CommandStatus */
    Req_STREAM_STORE          = 32,  /*!< Store a stream fragment, see ::TRX_Request_StreamStore */
    Req_STREAM_FLUSH          = 33,  /*!< Flushes a stream with all fragments, see ::TRX_Request_StreamFlush */
    Req_CONFIG_STORE          = 48,  /*!< Store a config fragment, see ::TRX_Request_ConfigStore */
    Req_CONFIG_PERSIST        = 49,  /*!< Persist a config stream to non volatile (NV) storage, see ::TRX_Request_ConfigPersist */
    Req_CONFIG_LIST           = 50,  /*!< List stored and persisted config streams, see ::TRX_Request_ConfigList */
    Req_CONFIG_LIST_RSP       = 51,  /*!< Response containing stored configs, see ::TRX_Request_ConfigListResponse */
    Req_CONFIG_FLUSH          = 52,  /*!< Flushes a config with all fragments, see ::TRX_Request_ConfigFlush */
    Req_DEVICE_CONFIG_STORE   = 56,  /*!< Store the device config, see ::TRX_Request_DeviceConfigStore */
    Req_DEVICE_CONFIG_PERSIST = 57,  /*!< Persist the device config to non volatile (NV) storage, see ::TRX_Request_DeviceConfigPersist */
    Req_DEVICE_CONFIG_GET     = 58,  /*!< Get stored or persisted device config, see ::TRX_Request_DeviceConfigGet */
    Req_DEVICE_CONFIG_GET_RSP = 59,  /*!< Response containing stored device config, see ::TRX_Request_DeviceConfigGetResponse */
    Req_UTIL_TIME             = 64,  /*!< Get-time utility request, see ::TRX_Request_UtilGetTime */
    Req_UTIL_RSSI             = 65,  /*!< Get-RSSI utility request, see ::TRX_Request_UtilGetRssi */
    Req_UTIL_INFO_VERSION     = 66,  /*!< Version utility request, see ::TRX_Request_UtilInfoGetVersion */
    Req_UTIL_STROBE           = 67,  /*!< Send strobe command as request, see ::TRX_Request_UtilStrobe */
    Req_UTIL_INFO_MAC_UUID    = 68,  /*!< MAC address and UUID utility request, see ::TRX_Request_UtilInfoGetMacUuid */
    Req_UTIL_INFO_DEVICE      = 69,  /*!< Device information utility request, see ::TRX_Request_UtilInfoGetDevice */
    Req_UTIL_ENTER_SSBL       = 70,  /*!< Request to enter secure bootloader upon next reset. The host is responsible for resetting the device after this command completes. */
    Req_UTIL_POWER_MODE       = 71,  /*!< Request to transition power mode, see ::TRX_Request_UtilPowerMode */
    Req_UTIL_RF_MODE          = 72,  /*!< Request to transition RF mode, see ::TRX_Request_UtilRfMode */
    Req_UTIL_SHUTDOWN         = 73,  /*!< Request to transition into shutdown power mode, see ::TRX_Request_UtilShutdown */
    Req_NV_ALLOCATE           = 80,  /*!< Allocate NV entry */
    Req_NV_WRITE              = 81,  /*!< Write to NV entry */
    Req_NV_FINALIZE           = 82,  /*!< Finalize item (can be found via GET) */
    Req_NV_INVALIDATE         = 83,  /*!< Invalidate item */
    Req_NV_GET                = 84,  /*!< Get item */
    Req_NV_INFO               = 85,  /*!< Get information about NV state */
    Req_NV_ERASE              = 86,  /*!< Erase all NV */
    Req_NV_CORRUPT            = 87,  /*!< Notice that NV consistency check has failed, need to erase NV */
    Req_DIO_SETUP             = 91,  /*!< Setup GPIO pins, see ::TRX_Request_DioSetup */
    Req_PING                  = 127, /*!< PING request, see ::TRX_Request_Ping */
} TRX_RequestType;

typedef struct TRX_Request_Header_s {
    uint8_t len       : 8;
    uint8_t request   : 8; /*!< Valid opcodes ::TRX_RequestType. Only the lowest 7 bits are used. The most significant bit is reserved */
} TRX_Request_Header;

/** Generic Request record
 *
 * These are contained within ::TRX_SpiBlock.payload
 * @note Start must be 32-bit aligned within the ::TRX_SpiBlock
 */
typedef struct TRX_Request {
    TRX_Request_Header header;
    uint8_t            data[];
} TRX_Request;

typedef enum TRX_Request_TransportError_Error_e {
    TRX_Request_TransportError_CRC = 0,
    TRX_Request_TransportError_RX_ALLOC_ERR = 1,
    TRX_Request_TransportError_TX_ALLOC_ERR = 2,
    TRX_Request_TransportError_TID_UNEXPECTED = 3,
} TRX_Request_TransportError_Error;

/** Error reporting request */
typedef struct TRX_Request_TransportError_s {
    TRX_Request_Header               header;           /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_TRANSPORT_ERROR */
    uint8_t                          tid_ok : 4;
    uint8_t                          tid_error   : 4;
    TRX_Request_TransportError_Error error       : 8;
} TRX_Request_TransportError;

#define TRX_Request_TransportError_maxAllocation (0)

/** Ping request, will respond differently depending on `pingData`
 *
 *  - `pingData == 0xAE`, responds with LastStatus and another PingRequest with inverted pingData
 *  - `pingData == 0xAB`, responds with LastStatus(status=TRX_STATUS_NOT_IMPLEMENTED) and does not consume the request, causing memory leak
 *  - `pingData == else`, consumes the request and responds with LastStatus(status==TRX_STATUS_SUCCESS)
 */
typedef struct TRX_Request_Ping_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_PING */
    uint8_t            pingData;
} TRX_Request_Ping;

#define TRX_Request_Ping_maxAllocation (1)

typedef struct TRX_Request_LastStatus_Param_Version_s {
    uint8_t  major;
    uint8_t  minor;
    uint8_t  patch;
    uint8_t  build;
    uint32_t hash;
} TRX_Request_LastStatus_Param_Version;
typedef struct TRX_Request_LastStatus_Param_Mac_Uuid_s {
    uint8_t  macAddress[8]; /*!< 64-bit device-unique IEEE MAC address */
    uint8_t  uuid[8];       /*!< 64-bit device-unique UUID (non-sequential across parts) */
} TRX_Request_LastStatus_Param_Mac_Uuid;

typedef struct TRX_Request_LastStatus_Param_Device_s {
    uint32_t  deviceId; /*!< 32-bit value unique per device family */
    uint32_t  partId;   /*!< 32-bit value unique per part number */
} TRX_Request_LastStatus_Param_Device;

typedef struct TRX_StreamDescriptor_s {
    uint16_t stream_id : 6;  /*!< Stream identifier as used in StreamStore */
    uint16_t persisted : 1;  /*!< Whether stream is persisted to NV */
    uint16_t reserved  : 9;  /*!< Reserved */
    uint16_t length;         /*!< Length of stream */
    uint32_t reference;      /*!< Host reference, unique identifier for stream content */
} TRX_StreamDescriptor;

typedef struct TRX_Request_LastStatus_Param_NvInfo_s {
    uint16_t capacity;
    uint16_t available;
    uint16_t utilized;
} TRX_Request_LastStatus_Param_NvInfo;

typedef union TRX_Request_LastStatus_Params_u {
    uint32_t getTimeRsp;                                 /*!< Response for ::TRX_Request_UtilGetTime, system time in microseconds at the time of Chip-Select line de-assert for the transaction containing the ::TRX_Request_UtilGetTime */
    uint8_t stopReqStatusCode;                           /*!< Response for ::TRX_Request_CommandStop, command status after stop is processed. Readable as ::TRX_CommandStatus */
    int8_t getRssiRsp;                                   /*!< Response for ::TRX_Request_UtilGetRssi. 127 indicates invalid sample, -128 to 126 are valid */
    uint8_t submitReqRsp;                                /*!< Response for ::TRX_Request_CommandSubmit, status of command after submitting for scheduling. Readable as ::TRX_CommandStatus, valid if status is SUCCESS */
    TRX_Request_LastStatus_Param_Version getVersionRsp;  /*!< Response for ::TRX_Request_UtilInfoGetVersion */
    TRX_Request_LastStatus_Param_Mac_Uuid getMacUuidRsp; /*!< Response for ::TRX_Request_UtilInfoGetMacUuid, IEEE MAC address and UUID of the device */
    TRX_Request_LastStatus_Param_Device getDeviceRsp;    /*!< Response for ::TRX_Request_UtilInfoGetDevice, general information of the device */
    TRX_Request_LastStatus_Param_NvInfo nvInfo;          /*!< Response for ::TRX_Request_NvInfo */
    uint8_t configListRsp;                               /*!< Response for ::TRX_Request_ConfigList. Total amount of stored configurations. */
} TRX_Request_LastStatus_Params;

/** Returned from TRX in response to any received request or strobe
 */
typedef struct TRX_Request_LastStatus_s {
    TRX_Request_Header header;     /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_LAST_STATUS */
    uint8_t            req    : 7; /*!< `[ 2][ 0:6]` The originating request */
    uint8_t            strobe : 1; /*!< `[ 2][   7]` Was the originating request a strobe command */
    uint8_t            tid    : 4; /*!< `[ 3][ 0:3]` Transaction-id of originating request */
    uint8_t            status : 4; /*!< `[ 3][ 4:7]` See ::RequestStatus */
    uint8_t            params[];   /*!< `[ 4][ .. ]` Variable length extra-response data, see ::TRX_Request_LastStatus_Params */
} TRX_Request_LastStatus;

/** Type for the ::TRX_Request_LastStatus.status field */
typedef enum RequestStatus_e {
    TRX_STATUS_SUCCESS           = 0,
    TRX_STATUS_FAILURE           = 2,
    TRX_STATUS_INVALID_PARAM     = 3,
    TRX_STATUS_INVALID_STATE     = 4,
    TRX_STATUS_INVALID_ALIGNMENT = 5,
    TRX_STATUS_NOT_IMPLEMENTED   = 15,
} RequestStatus;

/** Type for errors raised by TRX modules */
typedef enum {
    TRX_ERROR_NONE = 0, /*!< No error, status OK */
} TRX_Error;

/*
 *   _____                                          _            _____ _____
 *  / ____|                                        | |     /\   |  __ \_   _|
 * | |     ___  _ __ ___  _ __ ___   __ _ _ __   __| |    /  \  | |__) || |
 * | |    / _ \| '_ ` _ \| '_ ` _ \ / _` | '_ \ / _` |   / /\ \ |  ___/ | |
 * | |___| (_) | | | | | | | | | | | (_| | | | | (_| |  / ____ \| |    _| |_
 *  \_____\___/|_| |_| |_|_| |_| |_|\__,_|_| |_|\__,_| /_/    \_\_|   |_____|
 *
 */

/** Command identifiers available for use with radio requests */
typedef enum TRX_RadioCommands_e {
    TRX_RadioCommand_NoOp     = 0,
    TRX_RadioCommand_Transmit = 1,     /*!< Transmit operation, see ::TRX_Request_CommandStore and ::TRX_Request_CommandStore_Transmit_Params */
    TRX_RadioCommand_Receive  = 2,     /*!< Receive operation, see ::TRX_Request_CommandStore and ::TRX_Request_CommandStore_Receive_Params */
    TRX_RadioCommand_CarrierSense = 3, /*!< Carrier sense operation, not functional */
    TRX_RadioCommand_TransmitTest = 4, /*!< TransmitTest operation, see ::TRX_Request_CommandStore and ::TRX_Request_CommandStore_TransmitTest_Params */
    TRX_RadioCommand_ReceiveTest  = 5, /*!< ReceiveTest operation, see ::TRX_Request_CommandStore and ::TRX_Request_CommandStore_ReceiveTest_Params */
    TRX_RadioCommand_DualReceive = 6, /*!< Dual Receive operation, see ::TRX_Request_CommandStore and ::TRX_Request_CommandStore_DualReceive_Params */
    TRX_RadioCommand_ExtTransmit = 7, /*!< Extended Transmit operation, see ::TRX_Request_CommandStore and ::TRX_Request_CommandStore_ExtTransmit_Params */
    TRX_RadioCommand_ExtReceive  = 8, /*!< Extended Receive operation, see ::TRX_Request_CommandStore and ::TRX_Request_CommandStore_ExtReceive_Params */
} TRX_RadioCommands;

/** Search strategy for RX command. This parameter only applies to FSK PHYs. The feature is not supported on all other PHYs. */
typedef enum TRX_RxSearchStrategy_e {
    TRX_Rx_SearchStrategy_Sync          = 0, /*!< Detect sync only */
    TRX_Rx_SearchStrategy_Energy_Sync   = 1, /*!< Detect energy and Sync. */
    TRX_Rx_SearchStrategy_Preamble_Sync = 2, /*!< Detect preamble and Sync. @note If an infinite preamble is continuously detected, the RX commands will never terminate and has to be stopped by the host. */
    TRX_Rx_SearchStrategy_Energy_Preamble_Sync   = 3, /*!< Detect energy, Preamble and Sync. */
} TRX_RxSearchStrategy;

/** PA selector for Tx/TxTest command */
typedef enum TRX_TxPa_e {
    TRX_TxPa_Low  = 0, /*!< 16dBm Low PA */
    TRX_TxPa_High = 1, /*!< 22dBm High PA. This is the only supported PA when using modem ::TRX_RadioCommand_Modem_OFDM */
} TRX_TxPa;

/**  Type for tx power configuration */
typedef union
{
    struct {
        uint16_t ib    : 6;
        uint16_t gain  : 3;
        uint16_t gain2 : 2;
        uint16_t boost : 2;
        uint16_t reserved  : 3; /*!< Reserved */
    };
    uint16_t rawValue;
} TRX_TxPowerTable_Value;

/**  Type for tx power configuration for 22 dBm PA. Not complete. */
typedef union
{
    struct {
        uint32_t fixgain    : 8;
        uint32_t magnadj    : 8;
        uint32_t pagain     : 4;
        uint32_t daciref    : 4;
        uint32_t dacitrim   : 3;
        uint32_t filtattn   : 3;
        uint32_t reserved   : 2; /*!< Reserved */
    };
    uint32_t rawValue;
} TRX_TxPowerTableHigh_Value;

/**  TX power setting
 *
 *   The TX power settings allows specifying the power in 0.5 dB steps. The
 *   available power levels depend on the power table in the PHY settings.
 *   If a power is requested that is not in the table, the highest power
 *   setting below the requested level is used. If the requested power is
 *   outside the bounds of the table, an error is returned. The power levels
 *   available can be found in the powerLevels struct of the PHY settings, see
 *   ::TRX_TxPowerLevels.
 */
typedef union
{
    struct {
        uint8_t fraction : 1; /*!< If set to 1, raises the requested power level by 0.5 dB */
        int8_t dBm : 7; /*!< Unit of level used to indicate that a power level is expressed in decibels (dB) with reference to one milliwatt (mW) */
    };
    int8_t rawValue; /*!< rawValue is twice the dBm number, allowing 0.5 dB steps */
} TRX_TxPower;

/**  List of available TX power levels
 *
 *   List of available TX power levels in dBm with fractional part.
 *   A struct of this type is part of rcl_settings.c for applicable PHYs.
 */
typedef struct
{
    uint8_t numEntries;         /*!< Number of entries in the power table */
    TRX_TxPower powerLevels[]; /*!< PA power levels */
} TRX_TxPowerLevels;

/**
 *  Special value given as a TX power to indicate that the lowest available
 *  TX power is requested
 */
#define TRX_TxPower_Use_Min       ((TRX_TxPower){.rawValue = -128})
/**
 *  Special value given as a TX power to indicate that the highest available
 *  TX power is requested
 */
#define TRX_TxPower_Use_Max       ((TRX_TxPower){.rawValue = 125})
/**
 *  Special value given as a TX power to indicate that a given raw TX power
 *  register setting should be used.
 */
#define TRX_TxPower_Use_Raw       ((TRX_TxPower){.rawValue = 126})

/** Modes of TX Test command. */
typedef enum TRX_TxTestMode_e {
    TRX_TxTestMode_Unmodulated = 0, /*!< Unmodulated carrier wave at center frequency. */
    TRX_TxTestMode_Modulated   = 1, /*!< Unwhitened modulated 8-bit data pattern (FSK only). */
    TRX_TxTestMode_PN9         = 2, /*!< PN9 sequence. */
    TRX_TxTestMode_Packet      = 3, /*!< Normal packet transmission. */
} TRX_TxTestMode;

/** Modes of TX Test command repeat. */
typedef enum TRX_TxTestRepeat_e {
    TRX_TxTest_Repeat_None     = 0,      /*!< Send one packet */
    TRX_TxTest_Repeat_Infinite = 0xFFFF, /*!< Repeat until stopped */
} TRX_TxTestRepeat;

/** Value used to manually set power in TxTest command */
typedef union TRX_TxTestPowerRaw_u {
    struct {
        TRX_TxPowerTable_Value value;
        uint8_t tempCoeff;
    } power_raw_low_pa;          /*!< To be used when power == TRX_TxPower_Use_Raw and pa == TRX_TxPa_Low on CC140X family. */
    struct {
        TRX_TxPowerTableHigh_Value value;
        uint8_t tempCoeff;
        uint8_t reserved[2];
    } power_raw_high_pa;         /*!< To be used when power == TRX_TxPower_Use_Raw and pa == TRX_TxPa_High on CC140X family. */
} TRX_TxTestPowerRaw;

/** Modes of carrier sense command. */
typedef enum TRX_CarrierSenseMode_e {
    TRX_CarrierSense_Mode_Energy = 0,   /*!< Channel busy if energy above threshold */
    TRX_CarrierSense_Mode_Preamble = 1, /*!< Channel busy if preamble detected; only available with FSK modem (certain PHYs) */
    TRX_CarrierSense_Mode_And = 2,      /*!< Channel busy if energy above threshold and preamble detected; only available with FSK modem (certain PHYs) */
    TRX_CarrierSense_Mode_Or = 3,       /*!< Channel busy if energy above threshold or preamble detected; only available with FSK modem (certain PHYs) */
} TRX_CarrierSenseMode;

/** Modes of carrier sense command. */
typedef enum TRX_CarrierSenseExitCondition_e {
    TRX_CarrierSense_ExitCondition_WaitForIdle = 0, /*!< Finish on channel idle or timeout */
    TRX_CarrierSense_ExitCondition_WaitForBusy = 1, /*!< Finish on channel busy or timeout */
    TRX_CarrierSense_ExitCondition_Fast = 2,        /*!< Finish as soon as any result available */
} TRX_CarrierSenseExitCondition;

/** Marker for invalid RSSI sample that should be ignored.
 *  Also used for carrier sense RSSI threshold as ignore-override value.
 */
#define TRX_RSSI_INVALID (127)

/** RSSI threhshold enum for Carrier sense. */
typedef enum TRX_CarrierSenseRSSI_e {
    TRX_CarrierSenseRSSI_UsePHY = TRX_RSSI_INVALID, /*!< Don't override, use PHY setting. */
} TRX_CarrierSenseRSSI;

/** RSSI threshold enum for Extended Receive */
typedef enum TRX_ExtReceiveRssiThreshold_e {
    TRX_ExtReceiveRssiThreshold_UsePHY = TRX_RSSI_INVALID, /*!< Don't override, use PHY setting. */
} TRX_ExtReceiveRssiThreshold;

#define TRX_FIXED_LENGTH_INVALID (0xFFFF)

/** Fixed length enum for Extended Transmit and Receive */
typedef enum TRX_ExtTxRxFixedLength_e {
    TRX_ExtTxRxLength_UsePHY = TRX_FIXED_LENGTH_INVALID, /*!< Don't override, use PHY setting. */
} TRX_ExtTxRxFixedLength;

/** Selection of PHY and configuration */
typedef struct __attribute__((__packed__)) TRX_Phy_s {
    uint16_t option_mask;     /*!< Bitmask of activated phy features */
    uint8_t  config_id   : 4; /*!< 0: ignore, 1-15: config stored with ::TRX_Request_ConfigStore */
    uint8_t  reserved    : 4; /*!< Reserved */
} TRX_Phy;

/** Time-conversion helpers */
#define TRX_TIMING_TICKS_PER_US 1
#define TRX_TIMING_US_PER_TICK  1
#define TRX_TIMING_TICKS_PER_S  1e6
#define TRX_TIMING_S_PER_TICK   1e-6

/** Special value for frequency selection indicating use cached */
#define TRX_FREQUENCY_REUSE_PREVIOUS (0)

/** Parameters for ::TRX_RadioCommand_Transmit command
 *
 * A submitted and completed Transmit command returns ::TRX_Request_CommandStatus with no parameters.
 * If chaining is enabled, onTrue will be taken in all cases where the radio completed a transmission, otherwise onFalse is taken.
 */
typedef struct __attribute__((__packed__)) TRX_Request_CommandStore_Transmit_Params_s {
    TRX_Phy  phy0;              /*!< Mandatory */
    TRX_Phy  phy1;              /*!< Optional */
    TRX_Phy  phy2;              /*!< Optional */
    uint16_t stream_id;         /*!< Identifier to use in ::TRX_Request_StreamStore sent from TRX on RX*/
    uint32_t frequency    : 24; /*!< Frequency in kHz, eg 915 MHz -> 915000.
                                     If this value (interpreted as signed) is between -2 MHz to +2 MHz,
                                     it will be applied as a delta frequency on top of the most recently calibrated frequency (without re-calibration). */
    uint32_t reserved     : 4;  /*!< Not used */
    uint32_t pa           : 1;  /*!< The selected PA to be used for the command. See ::TRX_TxPa for supported values. */
    uint32_t modem        : 3;  /*!< See ::TRX_RadioCommand_Modem */
    TRX_TxPower power;          /*!< Transmit power. dBm value with one fractional bit used for stored PA table lookup of calibrated value, signed. */
} TRX_Request_CommandStore_Transmit_Params;

/** Parameters for ::TRX_RadioCommand_Receive command
 *
 * A submitted and completed Receive command returns ::TRX_Request_CommandStatus with parameters ::TRX_Request_CommandStatus_Params_Rx
 * appended. If chaining is enabled, onTrue will be taken in all cases where the radio completed a reception, otherwise onFalse is taken.
 */
typedef struct __attribute__((__packed__)) TRX_Request_CommandStore_Receive_Params_s {
    TRX_Phy  phy0;               /*!< Mandatory */
    TRX_Phy  phy1;               /*!< Optional */
    TRX_Phy  phy2;               /*!< Optional */
    uint16_t stream_id;          /*!< Identifier to use in ::TRX_Request_StreamStore sent from TRX on RX*/
    uint32_t frequency    : 24;  /*!< Frequency in kHz, eg 915 MHz -> 915000.
                                      If this value (interpreted as signed) is between -2 MHz to +2 MHz,
                                      it will be applied as a delta frequency on top of the most recently calibrated frequency (without re-calibration). */
    uint32_t reserved               : 2;    /*!< Not used */
    uint32_t discardStatusOnTimeout : 1;    /*!< If the command times out, discards the status. Can be used for Sniff mode/self chaining */
    uint32_t enable_mdr             : 1;    /*!< Enable automatic PHY switching when a WiSUN MDR header (modulation data rate switch) is received */
    uint32_t stream_early           : 1;    /*!< Start sending received frames over SPI immediately as soon as fragments are received over the air */
    uint32_t modem                  : 3;    /*!< See ::TRX_RadioCommand_Modem */
    uint32_t timeout;                       /*!< timeout, measured in microseconds. Timeouts will yield :: TRX_CommandStatus.TRX_CommandStatus_GracefulStopTimeout */
    uint8_t  search_strategy;               /*!< See ::TRX_RxSearchStrategy. This parameter only applies to FSK PHYs. The feature is not supported on any other PHYs. */
    uint8_t  repeat;                        /*!< Whether to remain in RX after reception of a packet. In such case, the command must be terminated by issuing a stop command. */
} TRX_Request_CommandStore_Receive_Params;

/** Parameters for ::TRX_RadioCommand_TransmitTest command
 *
 * A submitted and completed TransmitTest command returns ::TRX_Request_CommandStatus with parameters
 * ::TRX_Request_CommandStatus_Params_TxTest appended. If chaining is enabled, onTrue will be taken in all cases where the command ends
 * after a packet transmission is completed, otherwise onFalse is taken.
 */
typedef struct __attribute__((__packed__)) TRX_Request_CommandStore_TransmitTest_Params_s {
    TRX_Phy  phy0;              /*!< Mandatory */
    TRX_Phy  phy1;              /*!< Optional */
    TRX_Phy  phy2;              /*!< Optional */
    uint16_t stream_id;         /*!< Identifier to use in ::TRX_Request_StreamStore sent from TRX on RX */
    uint32_t frequency    : 24; /*!< Frequency in kHz, eg 915 MHz -> 915000.
                                     If this value (interpreted as signed) is between -2 MHz to +2 MHz,
                                     it will be applied as a delta frequency on top of the most recently calibrated frequency (without re-calibration). */
    uint32_t mode         : 2;  /*!< See ::TRX_TxTestMode */
    uint32_t reserved     : 2;  /*!< Reserved */
    uint32_t pa           : 1;  /*!< The selected PA to be used for the command. See ::TRX_TxPa for supported values. */
    uint32_t modem        : 3;  /*!< See ::TRX_RadioCommand_Modem */
    TRX_TxPower power;          /*!< Transmit power. dBm value with one fractional bit used for stored PA table lookup of calibrated value, signed. */
    uint8_t  pattern;           /*!< 1-byte pattern modulated during transmission (mode=1), or first byte of over-the-air header where it impacts coding of transmitted signal (mode=2) */
    uint16_t repeat;            /*!< See ::TRX_TxTestRepeat. Only valid for mode=TRX_TxTestMode_Packet. */
    TRX_TxTestPowerRaw power_raw; /*!< See ::TRX_TxTestPowerRaw. To be used when power == TRX_TxPower_Use_Raw */
} TRX_Request_CommandStore_TransmitTest_Params;

/** Parameters for ::TRX_RadioCommand_ReceiveTest command
 *
 * A submitted and completed ReceiveTest command returns ::TRX_Request_CommandStatus with parameters
 * ::TRX_Request_CommandStatus_Params_RxTest appended. If chaining is enabled, onTrue will be taken in all cases where the radio completed a
 * reception, otherwise onFalse is taken.
 */
typedef struct __attribute__((__packed__))  TRX_Request_CommandStore_ReceiveTest_Params_s {
    TRX_Phy  phy0;                      /*!< Mandatory */
    TRX_Phy  phy1;                      /*!< Optional */
    TRX_Phy  phy2;                      /*!< Optional */
    uint16_t stream_id;                 /*!< Identifier to use in ::TRX_Request_StreamStore sent from TRX on RX*/
    uint32_t frequency            : 24; /*!< Frequency in kHz, eg 915 MHz -> 915000.
                                             If this value (interpreted as signed) is between -2 MHz to +2 MHz,
                                             it will be applied as a delta frequency on top of the most recently calibrated frequency (without re-calibration). */
    uint32_t test_nosync            : 1;    /*!< Prevent sync, remain in RX indefinitely */
    uint32_t test_discard_packets   : 1;    /*!< Discard all received packets, useful for faster packet-error-rate testing */
    uint32_t discardStatusOnTimeout : 1;    /*!< If the command times out, discards the status. Can be used for Sniff mode/self chaining */
    uint32_t enable_mdr             : 1;    /*!< Enable automatic PHY switching when a WiSUN MDR header (modulation data rate switch) is received */
    uint32_t stream_early         : 1;  /*!< Start sending received frames over SPI immediately as soon as fragments are received over the air */
    uint32_t modem                : 3;  /*!< See ::TRX_RadioCommand_Modem */
    uint32_t timeout;                   /*!< Sync search timeout, measured in microseconds. Timeouts will yield :: TRX_CommandStatus.TRX_CommandStatus_GracefulStopTimeout */
    uint8_t  search_strategy;           /*!< 0: sync, 1: energy + sync, 2: preamble + sync, 3: energy + preamble + sync */
    uint8_t  repeat;                    /*!< Whether to remain in RX after reception of a packet. In such case, the command must be terminated by issuing a stop command.*/
} TRX_Request_CommandStore_ReceiveTest_Params;

/** Parameters for ::TRX_RadioCommand_CarrierSense command
 *
 * A submitted and completed CarrierSense command returns ::TRX_Request_CommandStatus with no parameters appended.
 * If chaining is enabled, onTrue will be taken if the channel is busy, otherwise onFalse is taken.
 */
typedef struct __attribute__((__packed__)) TRX_Request_CommandStore_CarrierSense_Params_s {
    TRX_Phy  phy0;                /*!< Mandatory */
    TRX_Phy  phy1;                /*!< Optional */
    TRX_Phy  phy2;                /*!< Optional */
    uint16_t rssi_window    : 4;  /*!< RSSI sample count to be considered for CS result */
    uint16_t rssi_count     : 4;  /*!< RSSI samples required to change state from the initial state */
    uint16_t corr_window    : 4;  /*!< RSSI sample count to be considered for CS result */
    uint16_t corr_count     : 4;  /*!< RSSI samples required to change state from the initial state */
    uint32_t frequency      : 24; /*!< Frequency in kHz, eg 915 MHz -> 915000.
                                       If this value (interpreted as signed) is between -2 MHz to +2 MHz,
                                       it will be applied as a delta frequency on top of the most recently calibrated frequency (without re-calibration). */
    uint32_t mode                   : 2;    /*!< See ::TRX_CarrierSenseMode */
    uint32_t exit_condition         : 2;    /*!< See ::TRX_CarrierSenseExitCondition */
    uint32_t discardStatusOnTimeout : 1;    /*!< If the command times out, discards the status. Can be used for Sniff mode/self chaining */
    uint32_t modem                  : 3;    /*!< See ::TRX_RadioCommand_Modem */
    uint32_t timeout;             /*!< CS timeout, measured in microseconds. 0 for no timeout. Timeouts will yield :: TRX_CommandStatus.TRX_CommandStatus_GracefulStopTimeout. Unused if exit_condition is Fast. */
    int32_t  rssi_override  : 8;  /*!< RSSI threshold value to override PHY config. See ::TRX_CarrierSenseRSSI */
    int32_t  reserved1      : 24; /*!< Not used */
} TRX_Request_CommandStore_CarrierSense_Params;

/** Parameters for ::TRX_RadioCommand_DualReceive command
 *
 * A submitted and completed Dual-Receive command returns ::TRX_Request_CommandStatus with parameters ::TRX_Request_CommandStatus_Params_Rx
 * appended. If chaining is enabled, onTrue will be taken in all cases where the radio completed a reception, otherwise onFalse is taken.
 * Upon a succesful packet reception a DualReceive command will return a packet starting with a ::TRX_PayloadHeader_DualRx header indicating
 * which modulation was used for reception. See ::TRX_PayloadHeader_DualRx for more details on the packet format following such header.
 */
typedef struct __attribute__((__packed__)) TRX_Request_CommandStore_DualReceive_Params_s {
    TRX_Phy  phy0;               /*!< Mandatory (must be valid dual PHY) */
    TRX_Phy  phy1;               /*!< Optional */
    TRX_Phy  phy2;               /*!< Optional */
    uint16_t stream_id;          /*!< Identifier to use in ::TRX_Request_StreamStore sent from TRX on RX*/
    uint32_t frequency0;         /*!< Frequency in Hz for the primary channel of the dual PHY. */
    uint32_t frequency1;         /*!< Frequency in Hz for the secondary channel of the dual PHY. */
    uint8_t  test_nosync            : 1;    /*!< Prevent sync, remain in RX indefinitely */
    uint8_t  test_discard_packets   : 1;    /*!< Discard all received packets, useful for faster packet-error-rate testing */
    uint8_t  discardStatusOnTimeout : 1;    /*!< If the command times out, discards the status. Can be used for Sniff mode/self chaining */
    uint8_t  enable_mdr             : 1;    /*!< Enable automatic PHY switching when a WiSUN MDR header (modulation data rate switch) is received */
    uint8_t  stream_early           : 1;    /*!< Start sending received frames over SPI immediately as soon as fragments are received over the air */
    uint8_t  modem                  : 3;    /*!< See ::TRX_RadioCommand_Modem */
    uint32_t timeout;            /*!< timeout, measured in microseconds. Timeouts will yield :: TRX_CommandStatus.TRX_CommandStatus_GracefulStopTimeout */
    uint8_t  search_strategy;    /*!< See ::TRX_RxSearchStrategy. This parameter only applies to FSK PHYs. The feature is not supported on any other PHYs. */
    uint8_t  repeat;             /*!< Whether to remain in RX after reception of a packet. In such case, the command must be terminated by issuing a stop command. */
} TRX_Request_CommandStore_DualReceive_Params;

/** Parameters for ::TRX_RadioCommand_ExtTransmit command
 *
 * A submitted and completed Extended Transmit command returns ::TRX_Request_CommandStatus with no parameters.
 * If chaining is enabled, onTrue will be taken in all cases where the radio completed a transmission, otherwise onFalse is taken.
 */
typedef struct __attribute__((__packed__)) TRX_Request_CommandStore_ExtTransmit_Params_s {
    TRX_Phy  phy0;              /*!< Mandatory */
    TRX_Phy  phy1;              /*!< Optional */
    TRX_Phy  phy2;              /*!< Optional */
    uint8_t stream_id;         /*!< Identifier to use in ::TRX_Request_StreamStore sent from TRX on RX*/
    uint32_t frequency;         /*!< Frequency in Hz.
                                      If this value (interpreted as signed) is between -2 MHz to +2 MHz,
                                      it will be applied as a delta frequency on top of the most recently calibrated frequency (without re-calibration). */
    uint32_t reserved     : 4;  /*!< Not used */
    uint32_t pa           : 1;  /*!< The selected PA to be used for the command. See ::TRX_TxPa for supported values. */
    uint32_t modem        : 3;  /*!< See ::TRX_RadioCommand_Modem */
    TRX_TxPower power;          /*!< Transmit power. dBm value with one fractional bit used for stored PA table lookup of calibrated value, signed. */
    uint16_t fixed_length;      /*!< With PHYs using fixed length: (0-8180) this value must be set to the length of the packets in bytes, including header and excluding CRC.
                                     With PHYs using variable packet length: this value is unused.
                                     See ::TRX_ExtTxRxLength_UsePHY to leave uninitialized and use PHY's default instead. */
} TRX_Request_CommandStore_ExtTransmit_Params;

/** Parameters for ::TRX_RadioCommand_ExtReceive command
 *
 * A submitted and completed Extended Receive command returns ::TRX_Request_CommandStatus with parameters ::TRX_Request_CommandStatus_Params_Rx
 * appended. If chaining is enabled, onTrue will be taken in all cases where the radio completed a reception, otherwise onFalse is taken.
 */
typedef struct __attribute__((__packed__)) TRX_Request_CommandStore_ExtReceive_Params_s {
    TRX_Phy  phy0;                          /*!< Mandatory */
    TRX_Phy  phy1;                          /*!< Optional */
    TRX_Phy  phy2;                          /*!< Optional */
    uint8_t  stream_id;                     /*!< Identifier to use in ::TRX_Request_StreamStore sent from TRX on RX*/
    uint32_t frequency;                     /*!< Frequency in Hz.
                                                If this value (interpreted as signed) is between -2 MHz to +2 MHz,
                                                it will be applied as a delta frequency on top of the most recently calibrated frequency (without re-calibration). */
    uint8_t  test_nosync            : 1;    /*!< Testing parameter. Prevent sync, remain in RX indefinitely */
    uint8_t  test_discard_packets   : 1;    /*!< Testing parameter. Discard all received packets, useful for faster packet-error-rate testing */
    uint8_t  discardStatusOnTimeout : 1;    /*!< If the command times out, discards the status. Can be used for Sniff mode/self chaining */
    uint8_t  enable_mdr             : 1;    /*!< Enable automatic PHY switching when a WiSUN MDR header (modulation data rate switch) is received */
    uint8_t  stream_early           : 1;    /*!< Start sending received frames over SPI immediately as soon as fragments are received over the air */
    uint8_t  modem                  : 3;    /*!< See ::TRX_RadioCommand_Modem */
    uint32_t timeout;                       /*!< timeout, measured in microseconds. Timeouts will yield :: TRX_CommandStatus.TRX_CommandStatus_GracefulStopTimeout */
    uint8_t  search_strategy;               /*!< See ::TRX_RxSearchStrategy. This parameter only applies to FSK PHYs. The feature is not supported on any other PHYs. */
    uint8_t  repeat;                        /*!< Whether to remain in RX after reception of a packet. In such case, the command must be terminated by issuing a stop command. */
    uint16_t fixed_length;                  /*!< With PHYs using fixed length: (0-8180) this value must be set to the length of the packets in bytes, including header and excluding CRC.
                                                With PHYs using variable packet length: this value will be used as the maximum allowed length (not including header nor CRC).
                                                Longer length packets will be automatically discarded and the command will return ::TRX_CommandStatus_RxErr.
                                                Set to 0 to disable length filtering.
                                                See ::TRX_ExtTxRxLength_UsePHY to leave uninitialized and use PHY's default instead. */
    int8_t   rssi_threshold;                /*!< Minimum RSSI threshold required to trigger busy command state when using search strategy TRX_Rx_SearchStrategy_Energy_Sync or
                                                TRX_Rx_SearchStrategy_Energy_Preamble_Sync::. See ::TRX_ExtReceiveRssiThreshold to leave uninitialized and use PHY's default instead */
} TRX_Request_CommandStore_ExtReceive_Params;

/** Union of available command-specific parameters for ::TRX_Request_CommandStore.params */
typedef union TRX_RadioCommand_Params {
    TRX_Request_CommandStore_Transmit_Params     tx;        /*!< `[20][ 0:48]` TX-only parameters */
    TRX_Request_CommandStore_Receive_Params      rx;        /*!< `[20][ 0:48]` RX-only parameters */
    TRX_Request_CommandStore_TransmitTest_Params txTest;    /*!< `[20][ 0:48]` TX-Test-only parameters */
    TRX_Request_CommandStore_ReceiveTest_Params  rxTest;    /*!< `[20][ 0:48]` RX-Test-only parameters */
    TRX_Request_CommandStore_CarrierSense_Params cs;        /*!< `[20][ 0:48]` CS-only parameters */
    TRX_Request_CommandStore_DualReceive_Params dualRx;     /*!< `[20][ 0:48]` Dual RX-only parameters */
    TRX_Request_CommandStore_ExtTransmit_Params extTx;      /*!< `[20][ 0:48]` Extended TX-only parameters */
    TRX_Request_CommandStore_ExtReceive_Params  extRx;      /*!< `[20][ 0:48]` Extended RX-only parameters */
} TRX_RadioCommand_Params;

/** Conflict resolution policy options */
typedef enum TRX_ConflictPolicy_e {
    TRX_ConflictPolicy_AlwaysInterrupt = 0, /*!< Always stop a running command if necessary to run this command */
    TRX_ConflictPolicy_Polite          = 1, /*!< Stop a command as fast as possible without losing active transfers */
    TRX_ConflictPolicy_NeverInterrupt  = 2, /*!< Never stop an ongoing command */
} TRX_ConflictPolicy;

/** Mask values for modem selection */
typedef enum TRX_RadioCommand_Modem_e {
    TRX_RadioCommand_Modem_Auto  = 0,      /*!< Automatic modem selection based on PHY configuration */
    TRX_RadioCommand_Modem_FSK   = 1 << 0, /*!< MCE */
    TRX_RadioCommand_Modem_OOK   = 1 << 0, /*!< MCE - alias of TRX_RadioCommand_Modem_FSK */
    TRX_RadioCommand_Modem_OFDM  = 1 << 1, /*!< OCE */
    TRX_RadioCommand_Modem_OQPSK = 1 << 2, /*!< OQE */
} TRX_RadioCommand_Modem;

/** Store a command for later submission to scheduler */
typedef struct TRX_Request_CommandStore_s {
    TRX_Request_Header header;           /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_CMD_STORE */
    uint8_t  slot;                       /*!< `[ 2][ 0:7]` Slot identifier where command is stored within the 6 available command slots. The valid range is 0-5. */
    uint8_t  cmd_id;                     /*!< `[ 3][ 0:7]` Command-ID this request is invoking, see ::TRX_RadioCommands */
    uint8_t  reserved0;                  /*!< `[ 4][ 0:7]` Reserved for future use */
    uint8_t  slot_on_true          : 7;  /*!< `[ 5][ 0:6]` Which command-slot to branch to for true */
    uint8_t  enable_on_true        : 1;  /*!< `[ 5][   7]` Enable chaining to another command on true */
    uint8_t  slot_on_false         : 7;  /*!< `[ 6][ 0:6]` Which command-slot to branch to for false */
    uint8_t  enable_on_false       : 1;  /*!< `[ 6][   7]` Enable chaining to another command  on false */
    uint8_t  slot_on_compare       : 6;  /*!< `[ 7][ 0:5]` Which command-slot to branch to for status match */
    uint8_t  enable_on_compare     : 2;  /*!< `[ 7][ 6:7]` Configure status-match chaining. See ::TRX_Command_StatusMatch */
    int16_t  next_status_compare;        /*!< `[ 8][0:15]` What status code to compare against for status-match branch */
    uint8_t  trigger               : 5;  /*!< `[10][ 0:4]` Trigger type for initial scheduling. See ::TRX_Command_TriggerType. Used when first in a chain. */
    uint8_t  conflict_policy       : 2;  /*!< `[10][ 5:6]` How to resolve conflicts, see ::TRX_ConflictPolicy */
    uint8_t  allow_delay           : 1;  /*!< `[10][   7]` Is a delayed start allowed? */
    uint8_t  chain_trigger         : 5;  /*!< `[11][ 0:4]` Trigger type for repeat scheduling. See ::TRX_Command_TriggerType. Only relevant when not first in a chain. */
    uint8_t  chain_conflict_policy : 2;  /*!< `[11][ 5:6]` How to resolve conflicts, see ::TRX_ConflictPolicy */
    uint8_t  chain_allow_delay     : 1;  /*!< `[11][   7]` Is a delayed start allowed? */
    uint32_t trigger_param;              /*!< `[12][0:31]` Scheduling parameter. Depends on trigger type. See ::TRX_Command_ScheduleParam */
    uint32_t chain_trig_param;           /*!< `[16][0:31]` Chained scheduling parameter. Depends on trigger type. See ::TRX_Command_ScheduleParam */
    TRX_RadioCommand_Params params;      /*!< `[20][ 0:n]` Command-specific parameters */
} TRX_Request_CommandStore;

#define TRX_Request_CommandStore_maxAllocation (0)

/** Options for chaining branch based on previous command status */
typedef enum TRX_Command_StatusMatch_e {
    Command_Status_IgnoreMatch  = 0, /*!< Disable the feature - comparison will not occur */
    Command_Status_StopOnMatch  = 1, /*!< Enable the feature - stop the chain when comparison is successful */
    Command_Status_EnableBranch = 2, /*!< Enable the feature - branch to the command-slot indicated by TRX_Request_CommandStore::slot_on_compare when comparison is successful */
} TRX_Command_StatusMatch;

/** Options for how command execution should be triggered after submission */
typedef enum TRX_Command_TriggerType_e {
    Command_Trigger_Immediate                    = 0,
    Command_Trigger_IoEdge                       = 2,
    Command_Trigger_Time_Absolute                = 3,  /*!< Absolute time in microseconds. */
    Command_Trigger_Stream                       = 5,  /*!< Only supported for TX commands. Start when the referenced stream is sent to the TRX device. */
    Command_Trigger_Time_Relative_Previous_Start = 10, /*!< Previous start is "sync found" for RX commands, and "command started" for non-RX commands. */
    Command_Trigger_Time_Relative_Previous_End   = 11,
} TRX_Command_TriggerType;

/** Trigger-configuration parameter, type depends on choice of ::TRX_Command_TriggerType */
typedef union TRX_Command_ScheduleParam_u {
    struct edge_trigger_config {
        uint16_t io_num;
        uint16_t edge_polarity;
    } edge;

    uint32_t timeAbs;                 /*!< Absolute time in microseconds. */
    uint32_t timeRelPrevStart;        /*!< Previous command start time in microseconds. See ::Command_Trigger_Time_Relative_Previous_Start */
    uint32_t timeRelPrevEnd;          /*!< Previous command end time in microseconds. See ::Command_Trigger_Time_Relative_Previous_End */
    uint32_t streamId;
    uint32_t raw;
} TRX_Command_ScheduleParam;

/** Submit a stored command to scheduler */
typedef struct TRX_Request_CommandSubmit_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_CMD_SUBMIT */
    uint8_t slot;              /*!< `[ 1][0:7]`  Command slot to submit for scheduling */
} TRX_Request_CommandSubmit;

/** Transmit and CarrierSense may result in just 1 allocation, being ::TRX_CommandStatus, while Receive may also allocate an additional block if it receives anything */
#define TRX_Request_CommandSubmit_maxAllocation (2)

/** Options for how command execution should be triggered after submission */
typedef enum TRX_Command_StopType_e {
    TRX_Command_StopType_None           = 0, /*!< No stop requested */
    TRX_Command_StopType_DescheduleOnly = 1, /*!< Stop the command if it has not started executing */
    TRX_Command_StopType_Graceful = 2, /*!< Stop the command gracefully, that is finish a packet or transaction in progress before ending */
    TRX_Command_StopType_Hard     = 3, /*!< Stop the command as soon as possible */
} TRX_Command_StopType;

/** Stop an ongoing or planned command
 *
 * This request returns extra parameters in the ::TRX_Request_LastStatus.params
 * array, which is the ::TRX_CommandStatus after the request has tried to stop
 * the command.
 *
 * A ::TRX_Request_CommandStatus will be also issued when the command execution
 * is finalized if the return status of this command is either
 *
 * * ::TRX_CommandStatus_Active - Stop did not take immediate effect
 * * ::TRX_CommandStatus_Finished - Command had already completed, so the stop had no effect
 * * ::TRX_CommandStatus_GracefulStopTimeout - Command was active but stopped now due to timeout
 * * ::TRX_CommandStatus_GracefulStopApi - Command was active but stopped now due to stop command
 * * ::TRX_CommandStatus_GracefulStopScheduling - Command was active but stopped now due to scheduling
 * * ::TRX_CommandStatus_HardStopApi - Command was active but stopped now due to stop command
 * * ::TRX_CommandStatus_HardStopScheduling - Command was active but stopped now due to scheduling
 * * ::TRX_CommandStatus_DescheduledApi - Command was planned to start next, but stopped now due to stop command
 * * ::TRX_CommandStatus_DescheduledScheduling - Command was planned to start next, but stopped now due to scheduling
 *
 * @return uint16_t ::TRX_CommandStatus
 */
typedef struct TRX_Request_CommandStop_s {
    TRX_Request_Header header;    /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_CMD_STOP */
    uint8_t slot;                 /*!< Which command-slot to stop */
    uint8_t stop_type;            /*!< What type of stop should be issued, see ::TRX_Command_StopType */
} TRX_Request_CommandStop;

/** Invoking this might cause a ::TRX_CommandStatus to be returned, but this should be budgeted in the CommandSubmit */
#define TRX_Request_CommandStop_maxAllocation (0)


/** RF command status */
typedef enum TRX_CommandStatus_e {
    TRX_CommandStatus_Idle                     = 0x00, /*!< Command is not yet run. This state is mandatory when submitting. */
    TRX_CommandStatus_Queued                   = 0x01, /*!< Command is queued, but pending end of previous command */
    TRX_CommandStatus_Scheduled                = 0x02, /*!< Command is scheduled, pending start time. */
    TRX_CommandStatus_Active                   = 0x03, /*!< Command is currently running. */
    TRX_CommandStatus_Suspended                = 0x04, /*!< Command is suspended and will resume once the othe running commands have finished */
    TRX_CommandStatus_Deferred                 = 0x05, /*!< Command is deferred. */
    TRX_CommandStatus_Finished                 = 0x10, /*!< Command has finished normally */
    TRX_CommandStatus_ChannelIdle              = 0x11, /*!< CS command has finished with channel idle */
    TRX_CommandStatus_ChannelBusy              = 0x12, /*!< CS command has finished with channel busy */
    TRX_CommandStatus_NoSync                   = 0x14, /*!< Command timed out waiting for a returned packet from peer device */
    TRX_CommandStatus_RxErr                    = 0x15, /*!< Command ended due to errors with the received packet (e.g, CRC errors) */
    TRX_CommandStatus_RejectedStart            = 0x16, /*!< Command was rejected start due to scheduling parameters */
    TRX_CommandStatus_UnexpectedMdrRx          = 0x17, /*!< Command ended because an MDR packet was received when we have MDR disabled */
    TRX_CommandStatus_DescheduledApi           = 0x31, /*!< Command was descheduled (by API) before starting running in the radio */
    TRX_CommandStatus_DescheduledScheduling    = 0x32, /*!< Command was descheduled (by Scheduling) before starting running in the radio */
    TRX_CommandStatus_GracefulStopTimeout      = 0x34, /*!< Command ended gracefully because the set timeout expired */
    TRX_CommandStatus_GracefulStopApi          = 0x35, /*!< Command ended because graceful stop command was sent */
    TRX_CommandStatus_GracefulStopScheduling   = 0x36, /*!< Command ended gracefully because another command was scheduled */
    TRX_CommandStatus_HardStopApi              = 0x39, /*!< Command ended because hard stop command was sent */
    TRX_CommandStatus_HardStopScheduling       = 0x3A, /*!< Command ended hard because another command was scheduled */
    TRX_CommandStatus_Error                    = 0x80, /*!< Command ended with unknown error */
    TRX_CommandStatus_Error_Setup              = 0x81, /*!< Command ended because of an error in the setup */
    TRX_CommandStatus_Error_Param              = 0x82, /*!< Command ended because of an error with a parameter */
    TRX_CommandStatus_Error_MissingTxBuffer    = 0x83, /*!< Command ended because no TX buffer was available when required */
    TRX_CommandStatus_Error_TxBufferCorruption = 0x84, /*!< Command ended because of errors in TX buffer structure */
    TRX_CommandStatus_Error_RxBufferCorruption = 0x85, /*!< Command ended because of errors in RX buffer structure */
    TRX_CommandStatus_Error_StartTooLate       = 0x86, /*!< Command ended because start time was in the past */
    TRX_CommandStatus_Error_TxFifo             = 0x87, /*!< Command ended because of underflow of TX FIFO */
    TRX_CommandStatus_Error_RxFifo             = 0x88, /*!< Command ended because of unsupported overflow of RX FIFO (no buffer to store packets) */
    TRX_CommandStatus_Error_Synth              = 0x89, /*!< Command ended because of synth programming error */
    TRX_CommandStatus_Error_UnknownOp          = 0x8A, /*!< Command ended because radio did not recognize command; probably wrong image */
    TRX_CommandStatus_Error_AlreadySubmitted   = 0x8B, /*!< Command not submitted because it was already scheduled or running */
} TRX_CommandStatus;

typedef struct TRX_Request_CommandStatus_Params_Rx_s {
    uint16_t num_sync;      /*!< Number of syncword correlation matches */
    uint16_t num_header_ok; /*!< Number of header checksums without error. If no checksum follows num_sync. */
    uint16_t num_crc_ok;    /*!< Number packet CRC verified OK */
    uint8_t  pending_rx;    /*!< Is there pending RX data still not sent over SPI for this command */
    uint8_t  reserved0;
} TRX_Request_CommandStatus_Params_Rx;

typedef struct TRX_Request_CommandStatus_Params_RxTest_s {
    uint16_t num_sync;      /*!< Number of syncword correlation matches */
    uint16_t num_header_ok; /*!< Number header checksums verified OK */
    uint16_t num_crc_ok;    /*!< Number packet CRC verified OK */
    uint8_t  pending_rx;    /*!< Is there pending RX data still not sent over SPI for this command */
    int8_t   last_rssi;     /*!< Last packet received signal strength indication */
    uint16_t freq_offset;   /*!< Last packet frequency offset value, parsing depends on modem used */
    uint16_t link_quality0; /*!< Last packet link quality value, parsing depends on modem used */
    uint16_t link_quality1; /*!< Last packet link quality value, parsing depends on modem used */
} TRX_Request_CommandStatus_Params_RxTest;

typedef struct TRX_Request_CommandStatus_Params_TxTest_s {
    uint32_t num_sent; /*!< Number of packets sent */
} TRX_Request_CommandStatus_Params_TxTest;

/** Command status extra parameters */
typedef union TRX_Request_CommandStatus_Params_u {
    TRX_Request_CommandStatus_Params_Rx     rx;
    TRX_Request_CommandStatus_Params_RxTest rxtest;
    TRX_Request_CommandStatus_Params_TxTest txtest;
} TRX_Request_CommandStatus_Params;

/** Enumeration for chaining result */
typedef enum TRX_Request_CommandStatus_ChainAction_e {
    TRX_Request_CommandStatus_ChainAction_None    = 0,
    TRX_Request_CommandStatus_ChainAction_OnTrue  = 1,
    TRX_Request_CommandStatus_ChainAction_OnFalse = 2,
    TRX_Request_CommandStatus_ChainAction_OnMatch = 3,
} TRX_Request_CommandStatus_ChainAction;

/** Command status generated after command is finished executing */
typedef struct TRX_Request_CommandStatus_s {
    TRX_Request_Header header;   /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_CMD_STATUS */
    uint8_t status;              /*!< @see ::TRX_CommandStatus */
    uint8_t command_id;          /*!< Type of operation, see ::TRX_RadioCommands */
    uint8_t slot;                /*!< Originating command slot */
    uint8_t chain_action;        /*!< If chaining, which branch was taken. See ::TRX_Request_CommandStatus_ChainAction */
    uint8_t chain_target;        /*!< Which command slot was branched to. Valid if chain_action != ::TRX_Request_CommandStatus_ChainAction_None */
    uint8_t reserved0;
    uint8_t params[];            /*!< Extra return parameters. See ::TRX_Request_CommandStatus_Params */
} TRX_Request_CommandStatus;

#define TRX_Request_CommandStatus_maxAllocation (0)

typedef enum TRX_PayloadHeader_Modulation_e {
    TRX_PayloadHeader_Modulation_FSK  = 0, /*!< MCE */
    TRX_PayloadHeader_Modulation_OFDM = 1, /*!< OCE */
    TRX_PayloadHeader_Modulation_OQPSK = 2, /*!< OQE */
} TRX_PayloadHeader_Modulation;

typedef enum TRX_PayloadHeader_SunOFDM_Rate_e {
    TRX_PayloadHeader_SunOFDM_Rate_MCS0 = 0, /*!< SunOFDM MCS0, BPSK, 1/2 FEC, 4x frequency repetition*/
    TRX_PayloadHeader_SunOFDM_Rate_MCS1 = 1, /*!< SunOFDM MCS1, BPSK, 1/2 FEC, 2x frequency repetition */
    TRX_PayloadHeader_SunOFDM_Rate_MCS2 = 2, /*!< SunOFDM MCS2, QPSK, 1/2 FEC, 2x frequency repetition */
    TRX_PayloadHeader_SunOFDM_Rate_MCS3 = 3, /*!< SunOFDM MCS3, QPSK, 1/2 FEC, No frequency repetition */
    TRX_PayloadHeader_SunOFDM_Rate_MCS4 = 4, /*!< SunOFDM MCS4, QPSK, 3/4 FEC, No frequency repetition */
    TRX_PayloadHeader_SunOFDM_Rate_MCS5 = 5, /*!< SunOFDM MCS5, 16-QAM, 1/2 FEC, No frequency repetition */
    TRX_PayloadHeader_SunOFDM_Rate_MCS6 = 6, /*!< SunOFDM MCS6, 16-QAM, 3/4 FEC, No frequency repetition */
} TRX_PayloadHeader_SunOFDM_Rate;

typedef enum TRX_PayloadHeader_LrOFDM_Rate_e {
    TRX_PayloadHeader_LrOFDM_Rate_DSSS2 = 0, /*!< LrOFDM, DSSS = 2, Data Rate = 4.0 kbps */
    TRX_PayloadHeader_LrOFDM_Rate_DSSS4 = 1, /*!< LrOFDM, DSSS = 4, Data Rate = 2.0 kbps */
    TRX_PayloadHeader_LrOFDM_Rate_DSSS6 = 2, /*!< LrOFDM, DSSS = 6, Data Rate = 1.4 kbps */
    TRX_PayloadHeader_LrOFDM_Rate_DSSS8 = 3, /*!< LrOFDM, DSSS = 8, Data Rate = 1.0 kbps */
} TRX_PayloadHeader_LrOFDM_Rate;

typedef enum TRX_PayloadHeader_SunOQPSK_RateMode_e {
    TRX_PayloadHeader_SunOQPSK_RateMode0_6_25kbps = 0, /*!< SunOQPSK rate mode 0, data rate 6.25 kb/s (Spreading Mode (8,1)0/1-DSSS) */
    TRX_PayloadHeader_SunOQPSK_RateMode1_12_5kbps = 1, /*!< SunOQPSK rate mode 1, data rate 12.5 kb/s (Spreading Mode (4,1)-DSSS) */
    TRX_PayloadHeader_SunOQPSK_RateMode2_25kbps   = 2, /*!< SunOQPSK rate mode 2, data rate 25 kb/s (Spreading Mode (2,1)-DSSS) */
    TRX_PayloadHeader_SunOQPSK_RateMode3_50kbps   = 3, /*!< SunOQPSK rate mode 3, data rate 50 kb/s (Spreading Mode none) */
} TRX_PayloadHeader_SunOQPSK_RateMode;

/** Common packet header fields
 *
 * These fields exist for all PHYs except GENFSK which is completely
 * user-defined.
 */
typedef struct TRX_PayloadHeader_Common_s {
    uint16_t length;
    uint16_t res0       : 6;
    uint16_t modulation : 2;
    uint16_t res1       : 8;
} TRX_PayloadHeader_Common;

/** OFDM packet */
typedef struct TRX_PayloadHeader_SunOFDM_s {
    uint16_t length;         /*!< Length of payload, not including extra packet metadata */
    uint16_t rate       : 4; /*!< Rate, see ::TRX_PayloadHeader_SunOFDM_Rate */
    uint16_t scrambler  : 2; /*!< Scrambler */
    uint16_t modulation : 2; /*!< Modulation used, Const(1) for OFDM. See ::TRX_PayloadHeader_Modulation. */
    uint16_t newPhyId   : 8; /*!< Phy ID of the PHY the payload is received on. Valid if RX operation was started on FSK phy. Will always be non-zero if valid. */
} TRX_PayloadHeader_SunOFDM;

/** OFDMLR packet */
typedef struct TRX_PayloadHeader_OFDMLR_s {
    uint8_t length;         /*!< Length of payload, not including extra packet metadata */
    uint8_t reserved0;      /*!< Reserved */
    uint8_t rate       : 2; /*!< Rate, see ::TRX_PayloadHeader_LrOFDM_Rate */
    uint8_t reserved1  : 4; /*!< Reserved */
    uint8_t modulation : 2; /*!< Modulation used, Const(1) for OFDM. See ::TRX_PayloadHeader_Modulation. */
    uint8_t reserved3;      /*!< Reserved */
} TRX_PayloadHeader_OFDMLR;

/** SunFSK packet
 *
 * For transmitted packets, mode_switch = 1 will ignore the length field, and requires
 * newPhyId to be valid. For mode_switch = 0, newPhyId is ignored and length is the
 * length of payload data following this header.
 * For received packets, mode_switch indicates if payload was received on alternate phy.
 */
typedef struct TRX_PayloadHeader_SunFSK_s {
    uint16_t length;          /*!< Length of payload, not including extra packet metadata */
    uint16_t whitening   : 1; /*!< Whitening enabled (1) or not (0) */
    uint16_t fcs_mode    : 1; /*!< CRC32 (0) or CRC16 (1) */
    uint16_t mode_switch : 1; /*!< Const(0) for transmit, but is 1 if payload was received on alternate phy */
    uint16_t res0        : 3; /*!< Not used */
    uint16_t modulation  : 2; /*!< Modulation used, Const(0) for FSK. See ::TRX_PayloadHeader_Modulation. */
    uint16_t newPhyId    : 8; /*!< Phy ID of the PHY the payload is received on. Valid if mode_switch==1 */
} TRX_PayloadHeader_SunFSK;

/** IEEE 802.15.4 SUN O-QPSK packet
 *
 * For transmitted packets, modulation is ignored
 */
typedef struct TRX_PayloadHeader_SunQQPSK_s {
    uint16_t length;         /*!< Length of payload, not including extra packet metadata */
    uint16_t rate_mode  : 2; /*!< Rate mode, see ::TRX_PayloadHeader_SunOQPSK_RateMode */
    uint16_t res0       : 4; /*!< Not used */
    uint16_t modulation : 2; /*!< Modulation used, Const(2) for OQPSK. See ::TRX_PayloadHeader_Modulation. */
    uint16_t res1       : 8; /*!< Not used */
} TRX_PayloadHeader_SUNOQPSK;

/** Internal Packet Header
 *
 * This packet header is converted to the appropriate over-the-air
 * header format for the phy being used.
 * This packet header doesn't apply to Generic FSK, for which ::TRX_PayloadHeader_GenFSK
 * should instead be used.
 */
typedef union TRX_PayloadHeader_u {
    TRX_PayloadHeader_Common   common;
    TRX_PayloadHeader_SunOFDM  ofdm;
    TRX_PayloadHeader_OFDMLR   ofdmlr;
    TRX_PayloadHeader_SunFSK   sunfsk;
    TRX_PayloadHeader_SUNOQPSK sunoqpsk;
    uint16_t                   length;
    uint32_t                   u32;
    uint8_t                    u8[4];
} TRX_PayloadHeader;


/** Default Generic FSK packet
 *
 * This packet header is the default format for GenFSK. The GenFSK PHY can be fully
 * customized by the user in SmartRF Studio, including changing the header format
 * and size of the length field. Therefore this struct must be changed by the user
 * if a different format is selected.
 */
typedef struct TRX_PayloadHeader_GenFSK_s {
    uint8_t length; /*!< Length of payload, not including extra packet metadata */
} TRX_PayloadHeader_GenFSK;

/** Internal Dual Receive header
 *
 * This packet header is used by the Dual Receive command to indicate which modulation
 * was used for packet reception.
 * Depending on the modulation used for receiving the packet, the corresponding
 * packet header format will follow the Dual Receive header.
 * See ::TRX_PayloadHeader_SUNOQPSK for packets received on ::TRX_RadioCommand_Modem_OQPSK modulation,
 * TRX_PayloadHeader_SUNOFDM for packets received on ::TRX_RadioCommand_Modem_OFDM modulation
 * and TRX_PayloadHeader_GenFSK for packets received on ::TRX_RadioCommand_Modem_FSK modulation.
 */
typedef struct TRX_PayloadHeader_DualRX_s {
    uint8_t modem; /*!< Modem used for the received packet. See ::TRX_RadioCommand_Modem */
} TRX_PayloadHeader_DualRx;

/*
 *   _____ _                                       _____ _____
 *  / ____| |                                /\   |  __ \_   _|
 * | (___ | |_ _ __ ___  __ _ _ __ ___      /  \  | |__) || |
 *  \___ \| __| '__/ _ \/ _` | '_ ` _ \    / /\ \ |  ___/ | |
 *  ____) | |_| | |  __/ (_| | | | | | |  / ____ \| |    _| |_
 * |_____/ \__|_|  \___|\__,_|_| |_| |_| /_/    \_\_|   |_____|
 *
 */

/** Options for how command execution should be triggered after submission */
typedef enum TRX_Stream_Retention_e {
    Stream_Retention_Flush_On_Success = 0,              /*!< Flush associated stream-requests if referencing command finishes successfully, otherwise retain all fragments for retransmission. */
    Stream_Retention_Flush_Streaming  = 1,              /*!< Flush stream-fragments as and when payload is first loaded into the radio */
    Stream_Retention_Flush_Never      = 2,              /*!< Retain stream payload also if command succeeds */
    Stream_Retention_Flush_Streaming_Continuous = 3,    /*!< Flush stream-fragments as and when payload is loaded first into the radio and when loaded after a pause to allow the radio to cacth up */
} TRX_Stream_Retention;

/** Store a stream-fragment */
typedef struct TRX_Request_StreamStore_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_STREAM_STORE */
    uint8_t stream_id : 6;     /*!< `[ 2][ 0:5]` Stream-identifier, decided by host */
    uint8_t start     : 1;     /*!< `[ 2][   6]` Whether this is the start of the stream */
    uint8_t md        : 1;     /*!< `[ 2][   7]` Whether more stream fragments should be expected */
    uint8_t retention;         /*!< `[ 3][ 0:7]` Retention policy. See ::TRX_Stream_Retention */
    uint8_t data[];            /*!< `[ 4][ 0:n]` Payload. Depending on chosen configuration may contain header with total length */
} TRX_Request_StreamStore;

#define TRX_Request_StreamStore_maxAllocation (0)

/** This define dictates the size of the fragments array in the Stream Manager. You cannot store more than this many fragments
 * in the system at one time, counting both stream and config fragments. Persistent configurations stored in NV are not subject
 * to this limitation.
 */
#define TRX_Request_StreamStore_maxNumFragments (32)

/** Flush a stream including all fragments */
typedef struct TRX_Request_StreamFlush_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_STREAM_FLUSH */
    uint8_t stream_id : 6;     /*!< `[ 2][ 0:5]` Stream-identifier, decided by host */
    uint8_t reserved  : 2;     /*!< `[ 2][ 6:7]` Reserved */
} TRX_Request_StreamFlush;

#define TRX_Request_StreamFlush_maxAllocation (0)

/*
 *   _____             __ _                  _____ _____
 *  / ____|           / _(_)           /\   |  __ \_   _|
 * | |     ___  _ __ | |_ _  __ _     /  \  | |__) || |
 * | |    / _ \| '_ \|  _| |/ _` |   / /\ \ |  ___/ | |
 * | |___| (_) | | | | | | | (_| |  / ____ \| |    _| |_
 *  \_____\___/|_| |_|_| |_|\__, | /_/    \_\_|   |_____|
 *                           __/ |
 *                          |___/
 */

/** Config-id for the config stream holding a PA table and other front-end configuration */
#define TRX_CONFIG_ID_FRONTEND 0

/** Namespace in NV for Config streams, available as 0xcf0n for n in 0 to f */
#define TRX_CONFIG_NV_NAMESPACE 0xcf00

/** Store a config fragment */
typedef struct TRX_Request_ConfigStore_s {
    TRX_Request_Header header;  /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_CONFIG_STORE */
    uint8_t  config_id : 6;     /*!< `[ 2][ 0:5]` Configuration slot identifier, decided by host.
                                Config_id = 0 is used to store/update device Front End(FE) settings. If FE settings are being stored/updated,
                                the lastStatus response for this request alone will be delayed till the Radio completes running of a setup cmd. */
    uint8_t  start     : 1;     /*!< `[ 2][   6]` Whether this is the start of the configuration data stream */
    uint8_t  md        : 1;     /*!< `[ 2][   7]` Whether more stream fragments should be expected */
    uint8_t  reserved;          /*!< `[ 3][ 0:7]` Reserved */
    uint32_t reference;         /*!< `[ 4][0:31]` Host-reference for the configuration data, hash or other unique identifier used for recognition in ::TRX_Request_ConfigList */
    uint8_t  data[];            /*!< `[ 8][ 0:n]` Configuration array. Must contain whole 32-bit words, but an array can otherwise be split across requests. */
} TRX_Request_ConfigStore;

#define TRX_Request_ConfigStore_maxAllocation (0)

/** Flush a stream including all fragments */
typedef struct TRX_Request_ConfigFlush_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_CONFIG_FLUSH */
    uint8_t config_id : 6;     /*!< `[ 2][ 0:5]` Config-identifier, decided by host */
    uint8_t reserved  : 2;     /*!< `[ 2][ 6:7]` Reserved */
} TRX_Request_ConfigFlush;

#define TRX_Request_ConfigFlush_maxAllocation (0)


/** Persist a configuration to NV */
typedef struct TRX_Request_ConfigPersist_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_CONFIG_PERSIST */
    uint8_t config_id : 6;     /*!< `[ 2][ 0:5]` Config-identifier, decided by host */
    uint8_t reserved  : 2;     /*!< `[ 2][ 6:7]` Reserved */
} TRX_Request_ConfigPersist;
#define TRX_Request_ConfigPersist_maxAllocation (0)


/** Request a list of stored configurations, see ::TRX_Request_LastStatus_Params.configListRsp */
typedef struct TRX_Request_ConfigList_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_CONFIG_LIST */
} TRX_Request_ConfigList;

#define TRX_Request_ConfigList_maxAllocation (1)

/** Return a list of stored configurations */
typedef struct TRX_Request_ConfigListResponse_s {
    TRX_Request_Header header;      /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_CONFIG_LIST_RSP */
    uint8_t start_at;               /*!< For pagination, this is the internal index of the first stream descriptor in this response */
    uint8_t count;                  /*!< Amount of stream descriptors included in this response */
    TRX_StreamDescriptor streams[]; /*!< Stream descriptors for stored and persisted streams */
} TRX_Request_ConfigListResponse;

#define TRX_Request_ConfigListResponse_maxAllocation (0)

/**
 * Single entry of the WiSUN MDR (modulation data rate) mapping table.
 * Maps a MDR header to a PHY option mask and modem. Always applies to phy0.
 */
typedef union {
    struct {
        uint8_t mdrByte;            /*!< MDR header byte */
        uint8_t ignoreMode : 1;     /*!< Ignore "mode" (bits 0-3) in comparison, used by OFDM */
        uint8_t channelPlanIdIdx : 3; /*!< ChannelPlanId index within the incrementing list of ChannelPlanIDs used in the current region. Used to find the next frequency. */
        uint8_t modem : 4;          /*!< Modem to use */
        uint16_t optionMask;        /*!< Option-mask to apply to current phy0 */
    };
    uint32_t raw;
} TRX_WisunMdrMapping_Entry;

#define TRX_WisunMdrMapping_Entry_NOT_FOUND ((TRX_WisunMdrMapping_Entry){.raw = 0xFFFFFFFF})

/** WiSUN MDR mapping table. Always applies to phy0. */
typedef struct {
    uint32_t   numEntries;
    TRX_WisunMdrMapping_Entry mappingTable[];
} TRX_WisunMdrMappingTable;

/** WiSUN MDR delta-frequencies table. */
typedef struct {
    uint8_t resolution;             /*!< Resolution (in kHz) for the entries. */
    int8_t  entry[];                /*!< Delta frequencies, resolution given by resolution field. */
} TRX_WisunMdrDeltaFrequenciesTable;

/*
 *  _____             _             _____             __ _                  _____ _____
 * |  __ \           (_)           / ____|           / _(_)           /\   |  __ \_   _|
 * | |  | | _____   ___  ___ ___  | |     ___  _ __ | |_ _  __ _     /  \  | |__) || |
 * | |  | |/ _ \ \ / / |/ __/ _ \ | |    / _ \| '_ \|  _| |/ _` |   / /\ \ |  ___/ | |
 * | |__| |  __/\ V /| | (_|  __/ | |___| (_) | | | | | | | (_| |  / ____ \| |    _| |_
 * |_____/ \___| \_/ |_|\___\___|  \_____\___/|_| |_|_| |_|\__, | /_/    \_\_|   |_____|
 *                                                          __/ |
 *                                                         |___/
 */

/** Namespace in NV for device config streams, available as 0xdc00 */
#define TRX_DEVICE_CONFIG_NV_NAMESPACE 0xdc00

/**
 *  Available IO Functions for DIO_0.
 *  Unless stated as input, the functions are setting the DIO as output.
 *  ^^ Preamble detect is available only when using carrier sense command or
 *     when using RX command with search strategy TRX_Rx_SearchStrategy_Preamble_Sync or TRX_Rx_SearchStrategy_Energy_Preamble_Sync.
 *  ^^ Energy Detect is only available when using carrier sense command or
 *     when using RX command with search strategy TRX_Rx_SearchStrategy_Energy_Sync or TRX_Rx_SearchStrategy_Energy_Preamble_Sync.
 */
typedef enum TRX_Dio0Config_e {
    DIO0_GPIO_SET_0                 = 0x00,  /*!< Set DIO as GPIO with value 0 */
    DIO0_GPIO_SET_1                 = 0x01,  /*!< Set DIO as GPIO with value 1 */
    DIO0_LOW_PA_AND_HIGH_PA         = 0x11,  /*!< Set DIO as Low PA and High PA control signal */
    DIO0_LOW_PA_AND_LNA             = 0x12,  /*!< Set DIO as Low PA and LNA control signal */
    DIO0_HIGH_PA_AND_LNA            = 0x13,  /*!< Set DIO as High PA and LNA control signal */
    DIO0_HIGH_PA_AND_LOW_PA_AND_LNA = 0x14,  /*!< Set DIO as High PA, Low PA and LNA control signal */
    DIO0_HIGH_Z                     = 0x30,  /*!< Set DIO as high impedance */
    DIO0_INPUT_TRIGGER              = 0x31,  /*!< Set DIO as input trigger */
    DIO0_SFD                        = 0x41,  /*!< Set DIO as SFD(Sync Found) event output */
    DIO0_ENERGY_DETECT              = 0x42,  /*!< Set DIO as Energy Detect^^ event output */
    DIO0_PREAMBLE_DETECT            = 0x43,  /*!< Set DIO as Preamble detect^^ event output */
    DIO0_LNA                        = 0x52,  /*!< Set DIO as LNA control signal */
    DIO0_LOW_PA                     = 0x53,  /*!< Set DIO as Low PA control signal */
    DIO0_HIGH_PA                    = 0x54,  /*!< Set DIO as High PA control signal */
    DIO0_RF_TRACE                   = 0x69,  /*!< Set DIO as RF Tracer output */
    DIO0_UNCHANGED                  = 0xFF,  /*!< Set DIO as unchanged */
} TRX_Dio0Config;

/**
 *  Available IO Functions for DIO_2
 *  Unless stated as input, the functions are setting the DIO as output.
 *  ^^ Preamble detect is available only when using carrier sense command or
 *     when using RX command with search strategy TRX_Rx_SearchStrategy_Preamble_Sync or TRX_Rx_SearchStrategy_Energy_Preamble_Sync.
 *  ^^ Energy Detect is only available when using carrier sense command or
 *     when using RX command with search strategy TRX_Rx_SearchStrategy_Energy_Sync or TRX_Rx_SearchStrategy_Energy_Preamble_Sync.
 */
typedef enum TRX_Dio2Config_e {
    DIO2_GPIO_SET_0                 = 0x00,  /*!< Set DIO as GPIO with value 0 */
    DIO2_GPIO_SET_1                 = 0x01,  /*!< Set DIO as GPIO with value 1 */
    DIO2_LOW_PA_AND_HIGH_PA         = 0x11,  /*!< Set DIO as Low PA and High PA control signal */
    DIO2_LOW_PA_AND_LNA             = 0x12,  /*!< Set DIO as Low PA and LNA control signal */
    DIO2_HIGH_PA_AND_LNA            = 0x13,  /*!< Set DIO as High PA and LNA control signal */
    DIO2_HIGH_PA_AND_LOW_PA_AND_LNA = 0x14,  /*!< Set DIO as High PA, Low PA and LNA control signal */
    DIO2_HIGH_Z                     = 0x30,  /*!< Set DIO as high impedance */
    DIO2_INPUT_TRIGGER              = 0x31,  /*!< Set DIO as input trigger */
    DIO2_SFD                        = 0x41,  /*!< Set DIO as SFD(Sync Found) event output */
    DIO2_ENERGY_DETECT              = 0x42,  /*!< Set DIO as Energy Detect^^ event output */
    DIO2_PREAMBLE_DETECT            = 0x43,  /*!< Set DIO as Preamble detect^^ event output */
    DIO2_LNA                        = 0x52,  /*!< Set DIO as LNA control signal */
    DIO2_LOW_PA                     = 0x53,  /*!< Set DIO as Low PA control signal */
    DIO2_HIGH_PA                    = 0x54,  /*!< Set DIO as High PA control signal */
    DIO2_RF_TRACE                   = 0x69,  /*!< Set DIO as RF Tracer output */
    DIO2_UNCHANGED                  = 0xFF,  /*!< Set DIO as unchanged */
} TRX_Dio2Config;

/**
 *  Available IO Functions for DIO_3
 *  Unless stated as input, the functions are setting the DIO as output.
 *  ^^ Preamble detect is available only when using carrier sense command or
 *     when using RX command with search strategy TRX_Rx_SearchStrategy_Preamble_Sync or TRX_Rx_SearchStrategy_Energy_Preamble_Sync.
 *  ^^ Energy Detect is only available when using carrier sense command or
 *     when using RX command with search strategy TRX_Rx_SearchStrategy_Energy_Sync or TRX_Rx_SearchStrategy_Energy_Preamble_Sync.
 */
typedef enum TRX_Dio3Config_e {
    DIO3_GPIO_SET_0                 = 0x00,  /*!< Set DIO as GPIO with value 0 */
    DIO3_GPIO_SET_1                 = 0x01,  /*!< Set DIO as GPIO with value 1 */
    DIO3_LOW_PA_AND_HIGH_PA         = 0x11,  /*!< Set DIO as Low PA and High PA control signal */
    DIO3_LOW_PA_AND_LNA             = 0x12,  /*!< Set DIO as Low PA and LNA control signal */
    DIO3_HIGH_PA_AND_LNA            = 0x13,  /*!< Set DIO as High PA and LNA control signal */
    DIO3_HIGH_PA_AND_LOW_PA_AND_LNA = 0x14,  /*!< Set DIO as High PA, Low PA and LNA control signal */
    DIO3_HIGH_Z                     = 0x30,  /*!< Set DIO as high impedance */
    DIO3_INPUT_TRIGGER              = 0x31,  /*!< Set DIO as input trigger */
    DIO3_SFD                        = 0x41,  /*!< Set DIO as SFD(Sync Found) event output */
    DIO3_ENERGY_DETECT              = 0x42,  /*!< Set DIO as Energy Detect^^ event output */
    DIO3_PREAMBLE_DETECT            = 0x43,  /*!< Set DIO as Preamble detect^^ event output */
    DIO3_LNA                        = 0x52,  /*!< Set DIO as LNA control signal */
    DIO3_LOW_PA                     = 0x53,  /*!< Set DIO as Low PA control signal */
    DIO3_HIGH_PA                    = 0x54,  /*!< Set DIO as High PA control signal */
    DIO3_RF_TRACE                   = 0x69,  /*!< Set DIO as RF Tracer output */
    DIO3_UNCHANGED                  = 0xFF,  /*!< Set DIO as unchanged */
} TRX_Dio3Config;

/**
 *  Available IO Functions for DIO_4
 *  Unless stated as input, the functions are setting the DIO as output.
 *  ^^ Preamble detect is available only when using carrier sense command or
 *     when using RX command with search strategy TRX_Rx_SearchStrategy_Preamble_Sync or TRX_Rx_SearchStrategy_Energy_Preamble_Sync.
 *  ^^ Energy Detect is only available when using carrier sense command or
 *     when using RX command with search strategy TRX_Rx_SearchStrategy_Energy_Sync or TRX_Rx_SearchStrategy_Energy_Preamble_Sync.
 */
typedef enum TRX_Dio4Config_e {
    DIO4_GPIO_SET_0                 = 0x00,  /*!< Set DIO as GPIO with value 0 */
    DIO4_GPIO_SET_1                 = 0x01,  /*!< Set DIO as GPIO with value 1 */
    DIO4_LOW_PA_AND_HIGH_PA         = 0x11,  /*!< Set DIO as Low PA and High PA control signal */
    DIO4_LOW_PA_AND_LNA             = 0x12,  /*!< Set DIO as Low PA and LNA control signal */
    DIO4_HIGH_PA_AND_LNA            = 0x13,  /*!< Set DIO as High PA and LNA control signal */
    DIO4_HIGH_PA_AND_LOW_PA_AND_LNA = 0x14,  /*!< Set DIO as High PA, Low PA and LNA control signal */
    DIO4_HIGH_Z                     = 0x30,  /*!< Set DIO as high impedance */
    DIO4_INPUT_TRIGGER              = 0x31,  /*!< Set DIO as input trigger */
    DIO4_SFD                        = 0x41,  /*!< Set DIO as SFD(Sync Found) event output */
    DIO4_ENERGY_DETECT              = 0x42,  /*!< Set DIO as Energy Detect^^ event output */
    DIO4_PREAMBLE_DETECT            = 0x43,  /*!< Set DIO as Preamble detect^^ event output */
    DIO4_LNA                        = 0x52,  /*!< Set DIO as LNA control signal */
    DIO4_LOW_PA                     = 0x53,  /*!< Set DIO as Low PA control signal */
    DIO4_HIGH_PA                    = 0x54,  /*!< Set DIO as High PA control signal */
    DIO4_RF_TRACE                   = 0x69,  /*!< Set DIO as RF Tracer output */
    DIO4_UNCHANGED                  = 0xFF,  /*!< Set DIO as unchanged */
} TRX_Dio4Config;

/**
 *  Available IO Functions for DIO_5
 *  Unless stated as input, the functions are setting the DIO as output.
 *  ^^ Preamble detect is available only when using carrier sense command or
 *     when using RX command with search strategy TRX_Rx_SearchStrategy_Preamble_Sync or TRX_Rx_SearchStrategy_Energy_Preamble_Sync.
 *  ^^ Energy Detect is only available when using carrier sense command or
 *     when using RX command with search strategy TRX_Rx_SearchStrategy_Energy_Sync or TRX_Rx_SearchStrategy_Energy_Preamble_Sync.
 */
typedef enum TRX_Dio5Config_e {
    DIO5_GPIO_SET_0                 = 0x00,  /*!< Set DIO as GPIO with value 0 */
    DIO5_GPIO_SET_1                 = 0x01,  /*!< Set DIO as GPIO with value 1 */
    DIO5_LOW_PA_AND_HIGH_PA         = 0x11,  /*!< Set DIO as Low PA and High PA control signal */
    DIO5_LOW_PA_AND_LNA             = 0x12,  /*!< Set DIO as Low PA and LNA control signal */
    DIO5_HIGH_PA_AND_LNA            = 0x13,  /*!< Set DIO as High PA and LNA control signal */
    DIO5_HIGH_PA_AND_LOW_PA_AND_LNA = 0x14,  /*!< Set DIO as High PA, Low PA and LNA control signal */
    DIO5_HIGH_Z                     = 0x30,  /*!< Set DIO as high impedance */
    DIO5_INPUT_TRIGGER              = 0x31,  /*!< Set DIO as input trigger */
    DIO5_SFD                        = 0x41,  /*!< Set DIO as SFD(Sync Found) event output */
    DIO5_ENERGY_DETECT              = 0x42,  /*!< Set DIO as Energy Detect^^ event output */
    DIO5_PREAMBLE_DETECT            = 0x43,  /*!< Set DIO as Preamble detect^^ event output */
    DIO5_LNA                        = 0x52,  /*!< Set DIO as LNA control signal */
    DIO5_LOW_PA                     = 0x53,  /*!< Set DIO as Low PA control signal */
    DIO5_HIGH_PA                    = 0x54,  /*!< Set DIO as High PA control signal */
    DIO5_RF_TRACE                   = 0x69,  /*!< Set DIO as RF Tracer output */
    DIO5_UNCHANGED                  = 0xFF,  /*!< Set DIO as unchanged */
} TRX_Dio5Config;

typedef enum TRX_Dio6Config_e {
    DIO6_GPIO_SET_0                 = 0x00,  /*!< Set DIO as GPIO with value 0 */
    DIO6_GPIO_SET_1                 = 0x01,  /*!< Set DIO as GPIO with value 1 */
    DIO6_LOW_PA_AND_HIGH_PA         = 0x11,  /*!< Set DIO as Low PA and High PA control signal */
    DIO6_LOW_PA_AND_LNA             = 0x12,  /*!< Set DIO as Low PA and LNA control signal */
    DIO6_HIGH_PA_AND_LNA            = 0x13,  /*!< Set DIO as High PA and LNA control signal */
    DIO6_HIGH_PA_AND_LOW_PA_AND_LNA = 0x14,  /*!< Set DIO as High PA, Low PA and LNA control signal */
    DIO6_HIGH_Z                     = 0x30,  /*!< Set DIO as high impedance */
    DIO6_INPUT_TRIGGER              = 0x31,  /*!< Set DIO as input trigger */
    DIO6_SFD                        = 0x41,  /*!< Set DIO as SFD(Sync Found) event output */
    DIO6_ENERGY_DETECT              = 0x42,  /*!< Set DIO as Energy Detect^^ event output */
    DIO6_PREAMBLE_DETECT            = 0x43,  /*!< Set DIO as Preamble detect^^ event output */
    DIO6_LNA                        = 0x52,  /*!< Set DIO as LNA control signal */
    DIO6_LOW_PA                     = 0x53,  /*!< Set DIO as Low PA control signal */
    DIO6_HIGH_PA                    = 0x54,  /*!< Set DIO as High PA control signal */
    DIO6_RF_TRACE                   = 0x69,  /*!< Set DIO as RF Tracer output */
    DIO6_UNCHANGED                  = 0xFF,  /*!< Set DIO as unchanged */
} TRX_Dio6Config;

/**
 *  Available IO Functions for DIO_7
 *  Unless stated as input, the functions are setting the DIO as output.
 *  ^^ Preamble detect is available only when using carrier sense command or
 *     when using RX command with search strategy TRX_Rx_SearchStrategy_Preamble_Sync or TRX_Rx_SearchStrategy_Energy_Preamble_Sync.
 *  ^^ Energy Detect is only available when using carrier sense command or
 *     when using RX command with search strategy TRX_Rx_SearchStrategy_Energy_Sync or TRX_Rx_SearchStrategy_Energy_Preamble_Sync.
 */
typedef enum TRX_Dio7Config_e {
    DIO7_GPIO_SET_0                 = 0x00,  /*!< Set DIO as GPIO with value 0 */
    DIO7_GPIO_SET_1                 = 0x01,  /*!< Set DIO as GPIO with value 1 */
    DIO7_LOW_PA_AND_HIGH_PA         = 0x11,  /*!< Set DIO as Low PA and High PA control signal */
    DIO7_LOW_PA_AND_LNA             = 0x12,  /*!< Set DIO as Low PA and LNA control signal */
    DIO7_HIGH_PA_AND_LNA            = 0x13,  /*!< Set DIO as High PA and LNA control signal */
    DIO7_HIGH_PA_AND_LOW_PA_AND_LNA = 0x14,  /*!< Set DIO as High PA, Low PA and LNA control signal */
    DIO7_HIGH_Z                     = 0x30,  /*!< Set DIO as high impedance */
    DIO7_INPUT_TRIGGER              = 0x31,  /*!< Set DIO as input trigger */
    DIO7_SFD                        = 0x41,  /*!< Set DIO as SFD(Sync Found) event output */
    DIO7_ENERGY_DETECT              = 0x42,  /*!< Set DIO as Energy Detect^^ event output */
    DIO7_PREAMBLE_DETECT            = 0x43,  /*!< Set DIO as Preamble detect^^ event output */
    DIO7_LNA                        = 0x52,  /*!< Set DIO as LNA control signal */
    DIO7_LOW_PA                     = 0x53,  /*!< Set DIO as Low PA control signal */
    DIO7_HIGH_PA                    = 0x54,  /*!< Set DIO as High PA control signal */
    DIO7_RF_TRACE                   = 0x69,  /*!< Set DIO as RF Tracer output */
    DIO7_UNCHANGED                  = 0xFF,  /*!< Set DIO as unchanged */
} TRX_Dio7Config;

/*
 * Configuration structure for Available DIOs
 */
typedef struct TRX_DioConfig_s {
    TRX_Dio0Config  dio0;         /*!< Configuration for dio0 */
    TRX_Dio2Config  dio2;         /*!< Configuration for dio2 */
    TRX_Dio3Config  dio3;         /*!< Configuration for dio3 */
    TRX_Dio4Config  dio4;         /*!< Configuration for dio4 */
    TRX_Dio5Config  dio5;         /*!< Configuration for dio5 */
    TRX_Dio6Config  dio6;         /*!< Configuration for dio6 */
    TRX_Dio7Config  dio7;         /*!< Configuration for dio7 */
} TRX_DioConfig;

/** Options for supported power modes */
typedef enum TRX_PowerPolicy_e {
    TRX_PowerPolicy_StandbyDisallow   = 0, /* TRX is not allowed to enter standby mode */
    TRX_PowerPolicy_StandbyAllow      = 1, /* TRX will automatically enter standby mode when possible */
} TRX_PowerPolicy;

typedef struct __attribute__((__packed__)) TRX_PowerMode_s {
    uint16_t        dwellTimeUs; /*!< Dwell time in microseconds before entering standby after CS deassert */
    TRX_PowerPolicy powerPolicy; /*!< Power policy for automatic transition to standby mode */
} TRX_PowerMode;

/** Options for supported RF modes */
typedef enum TRX_RfMode_e {
    TRX_RfMode_SUN              = 0, /*!< Load RF firmware to support SunFSK, SunOFSDM, LrOFDM, and SunOQPSK */
    TRX_RfMode_PropFSK          = 1, /*!< Load RF firmware to support PropFSK with standard AGC */
    TRX_RfMode_OOK              = 2, /*!< Load RF firmware to support OOK */
    TRX_RfMode_XLR              = 3, /*!< Load RF firmware to support XLR */
    TRX_RfMode_WMBus            = 4, /*!< Load RF firmware to support Wireless M-Bus */
    TRX_RfMode_PropFSKPeakAGC   = 6, /*!< Load RF firmware to support PropFSK with peak based AGC */
    TRX_RfMode_SUNPeakAGC       = 7, /*!< Load RF firmware to support SUN and SUNOFDM, LrOFDM, and SunOQPSK peak based AGC */
} TRX_RfMode;

typedef enum TRX_ClockConfig_e {
    TRX_ClockConfig_RCOSC    = 0, /*!< Use RC oscillator as clock source. RF not supported; only used until clock source has been selected */
    TRX_ClockConfig_XOSC     = 1, /*!< Use crystal oscillator as clock source */
    TRX_ClockConfig_TCXO     = 2, /*!< Use external TCXO as clock source */
} TRX_ClockConfig;

/** Setup contents */
typedef struct __attribute__((__packed__)) TRX_DeviceConfigData_s {
    TRX_DioConfig   dioConfig;      /*!< Initial DIO setup */
    TRX_ClockConfig clockConfig;    /*!< Clock source setup */
    TRX_PowerMode   powerMode;      /*!< Initial power mode setup */
    TRX_RfMode      rfMode;         /*!< Initial RF mode */
} TRX_DeviceConfigData;

/** Store a device config */
typedef struct TRX_Request_DeviceConfigStore_s {
    TRX_Request_Header header;      /*!< Request header. ::TRX_Request_Header.request must be ::Req_DEVICE_CONFIG_STORE */
    TRX_DeviceConfigData data;      /*!< Device Configuration structure, see ::TRX_DeviceConfigData for details */
} TRX_Request_DeviceConfigStore;

#define TRX_Request_DeviceConfigStore_maxAllocation (0)


/** Persist the device config to NV */
typedef struct TRX_Request_DeviceConfigPersist_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_DEVICE_CONFIG_PERSIST */
    uint8_t allowReplace: 1;   /*!< `[ 2][0]`    Whether an existing persisted config should be replaced if it exists. If 0 and a persisted config already exists, the request will fail */
    uint8_t reserved: 7;       /*!< `[ 2][1:7]`  Reserved */
} TRX_Request_DeviceConfigPersist;

#define TRX_Request_DeviceConfigPersist_maxAllocation (0)

/** Request current device config */
typedef struct TRX_Request_DeviceConfigGet_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_DEVICE_CONFIG_GET */
} TRX_Request_DeviceConfigGet;

#define TRX_Request_DeviceConfigGet_maxAllocation (1)

/** Return stored device config */
typedef struct  __attribute__((__packed__)) TRX_Request_DeviceConfigGetResponse_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_DEVICE_CONFIG_GET_RSP */
    uint8_t persisted: 1;      /*!< `[ 2][ 0]` Whether device config is persisted to NV */
    uint8_t modified: 1;       /*!< `[ 2][ 1]` Whether the stored configuration has been modified since last persist or erase */
    uint8_t reserved0: 6;      /*!< `[ 2][2:7]` Reserved */
    uint8_t reserved1;         /*!< `[ 3][0:7]` Reserved */
    TRX_DeviceConfigData data; /*!< `[ 4][ 0:n]` Returned configuration structure, see ::TRX_DeviceConfigData */
} TRX_Request_DeviceConfigGetResponse;

#define TRX_Request_DeviceConfigGetResponse_maxAllocation (0)

/*
 *  _    _ _   _ _            _____ _____
 * | |  | | | (_) |     /\   |  __ \_   _|
 * | |  | | |_ _| |    /  \  | |__) || |
 * | |  | | __| | |   / /\ \ |  ___/ | |
 * | |__| | |_| | |  / ____ \| |    _| |_
 *  \____/ \__|_|_| /_/    \_\_|   |_____|
 *
 */

/** Get TRX time at CSN deassert
 *
 * Returns ::TRX_Request_LastStatus with ::TRX_Request_LastStatus.params
 * readable as ::TRX_Request_LastStatus_Params.getTimeRsp
 */
typedef struct TRX_Request_UtilGetTime_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_UTIL_TIME */
} TRX_Request_UtilGetTime;

#define TRX_Request_UtilGetTime_maxAllocation (0)

/** Get current RSSI value from radio during RX. Diagnostic purposes only!
 *
 * Returns ::TRX_Request_LastStatus with ::TRX_Request_LastStatus.params
 * readable as ::TRX_Request_LastStatus_Params.getRssiRsp
 */
typedef struct TRX_Request_UtilGetRssi_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_UTIL_RSSI */
} TRX_Request_UtilGetRssi;

#define TRX_Request_UtilGetRssi_maxAllocation (0)

/** Get firmware version of the device
 *
 * Returns ::TRX_Request_LastStatus with ::TRX_Request_LastStatus.params
 * readable as ::TRX_Request_LastStatus_Params.getVersionRsp
 */
typedef struct TRX_Request_UtilInfoGetVersion_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_UTIL_INFO_VERSION */
} TRX_Request_UtilInfoGetVersion;

#define TRX_Request_UtilGetVersion_maxAllocation (0)

/** Encapsulate a strobe command in a request
 *
 * Returns ::TRX_Request_LastStatus
 */
typedef struct TRX_Request_UtilStrobe_s {
    TRX_Request_Header header;  /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_UTIL_STROBE */
    TRX_StrobeCommand  strobe;  /*!< `[ 2][0: 7]` Strobe command identifier */
    TRX_StrobeParam    param;   /*!< `[ 3][0: 7]` Strobe command parameter */
} TRX_Request_UtilStrobe;

#define TRX_Request_UtilStrobe_maxAllocation (0)

/** Get IEEE MAC address and UUID of the device
 *
 * Returns ::TRX_Request_LastStatus with ::TRX_Request_LastStatus.params
 * Readable as ::TRX_Request_LastStatus_Params.getMacUuidRsp
 */
typedef struct TRX_Request_UtilInfoGetMacUuid_s {
    TRX_Request_Header header;  /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_UTIL_INFO_MAC_UUID */
} TRX_Request_UtilInfoGetMacUuid;

#define TRX_Request_UtilInfoGetMacUuid_maxAllocation (0)

/** Get general device information
 *
 * Returns ::TRX_Request_LastStatus with ::TRX_Request_LastStatus.params
 * Readable as ::TRX_Request_LastStatus_Params.getDeviceRsp
 */
typedef struct TRX_Request_UtilInfoGetDevice_s {
    TRX_Request_Header header;  /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_UTIL_INFO_DEVICE */
} TRX_Request_UtilInfoGetDevice;

#define TRX_Request_UtilInfoGetDevice_maxAllocation (0)

/** Command to enter SSBL bootloader upon reset
 *
 * Returns ::TRX_Request_LastStatus
 */
typedef struct TRX_Request_UtilEnterSsbl_s {
    TRX_Request_Header header;  /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_UTIL_ENTER_SSBL */
} TRX_Request_UtilEnterSsbl;

#define TRX_Request_UtilEnterSsbl_maxAllocation (0)

/** Command to configure power mode
 *
 * Configure if the TRX is allowed to automatically enter standby mode, and if so the dwell time in
 *  microseconds to wait after the SPI chip select is deasserted before entering standby mode.
 *
 * Returns ::TRX_Request_LastStatus
 */
typedef struct TRX_Request_UtilPowerMode_s {
    TRX_Request_Header    header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_UTIL_POWER_MODE */
    TRX_PowerMode mode;   /*!< `[ 2][0:23]` Power mode configuration. See ::TRX_PowerMode for valid choices. */
} TRX_Request_UtilPowerMode;

#define TRX_Request_UtilPowerMode_maxAllocation (0)

/** Command to transition RF mode
 *
 * The command will transition the device to the requested RF mode.
 */

typedef struct TRX_Request_UtilRfMode_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_UTIL_RF_MODE */
    TRX_RfMode         mode;   /*!< `[ 2][0: 7]` RF mode selector. See ::TRX_RfMode for valid choices. */
} TRX_Request_UtilRfMode;

/** @brief Requests the TRX to transition into shutdown power mode.
 *
 * Returns ::TRX_Request_LastStatus
 */
typedef struct TRX_Request_UtilShutdown_s {
    TRX_Request_Header    header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_UTIL_SHUTDOWN */
} TRX_Request_UtilShutdown;

#define TRX_Request_UtilShutdown_maxAllocation (0)

/*  _   _                          _       _   _ _             _
 * | \ | |                        | |     | | (_) |           | |
 * |  \| | ___  _ __   __   _____ | | __ _| |_ _| | ___    ___| |_ ___  _ __ __ _  __ _  ___
 * | . ` |/ _ \| '_ \  \ \ / / _ \| |/ _` | __| | |/ _ \  / __| __/ _ \| '__/ _` |/ _` |/ _ \
 * | |\  | (_) | | | |  \ V / (_) | | (_| | |_| | |  __/  \__ \ || (_) | | | (_| | (_| |  __/
 * |_| \_|\___/|_| |_|   \_/ \___/|_|\__,_|\__|_|_|\___|  |___/\__\___/|_|  \__,_|\__, |\___|
 *                                                                                 __/ |
 *                                                                                |___/
 */

/** Mask for namespacing record-id's to modules, highest 8 bits are module ID */
#define TRX_NV_MODULE_NAMESPACE_MASK 0xff00

typedef struct TRX_Request_NvAllocate_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_NV_ALLOCATE */
    uint16_t record_id;
    uint16_t length;
} TRX_Request_NvAllocate;

#define TRX_Request_NvAllocate_maxAllocation (0)

typedef struct TRX_Request_NvWrite_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_NV_WRITE */
    uint16_t record_id;
    uint16_t offset;
    uint16_t reserved;
    uint8_t data[];
} TRX_Request_NvWrite;

#define TRX_Request_NvWrite_maxAllocation (0)

typedef struct TRX_Request_NvFinalize_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_NV_FINALIZE */
    uint16_t record_id;
} TRX_Request_NvFinalize;

#define TRX_Request_NvFinalize_maxAllocation (0)

typedef struct TRX_Request_NvInvalidate_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_NV_INVALIDATE */
    uint16_t record_id;
} TRX_Request_NvInvalidate;

#define TRX_Request_NvInvalidate_maxAllocation (0)

/** Get all records matching the wanted record_id and mask
 *
 * Will return a sequence of ::Req_NV_ALLOCATE and ::Req_NV_WRITE for each
 * record that matches.
 *
 * For exact match use mask 0xFFFF. To get all stored records use mask 0x0000.
 * For partial namespace match, use a mask and record_id where the mask has bits
 * set in the namespace-unique portion of the record_id and and cleared
 * otherwise.
 */
typedef struct TRX_Request_NvGet_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_NV_GET */
    uint16_t record_id;
    uint16_t record_id_mask;
    uint16_t reserved;
} TRX_Request_NvGet;

#define TRX_Request_NvGet_maxAllocation (0)

typedef struct TRX_Request_NvErase_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_NV_ERASE */
} TRX_Request_NvErase;

#define TRX_Request_NvErase_maxAllocation (0)

typedef struct TRX_Request_NvInfo_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_NV_INFO */
} TRX_Request_NvInfo;

#define TRX_Request_NvInfo_maxAllocation (0)

typedef struct TRX_Request_NvCorrupt_s {
    TRX_Request_Header header; /*!< `[ 0][0:15]` Request header. ::TRX_Request_Header.request must be ::Req_NV_CORRUPT */
} TRX_Request_NvCorrupt;

#define TRX_Request_NvCorrupt_maxAllocation (0)

/** IO Setup request with an available IO assigned for each required IO function
 *  TRX returns TRX_STATUS_SUCCESS if the DIO setup was succesfully handled.
 *  TRX_STATUS_INVALID_PARAM is returned instead if the parameters of the IO setup command were incorrect.
 *  TRX_STATUS_INVALID_STATE is returned if a radio command is running or scheduled when the IO setup request was received.
 *
 *  The host should issue a stop command towards the running/scheduled command slot before
 *  issuing a IO mux request.
 *
 *  Returns ::TRX_Request_LastStatus
 */
typedef struct TRX_Request_DioSetup_s {
    TRX_Request_Header header;          /*!< Request header. ::TRX_Request_Header.request must be ::Req_DIO_SETUP */
    TRX_DioConfig dioConfig;            /*!< Dio configuration. See ::TRX_DioConfig for details. */
} TRX_Request_DioSetup;

#define TRX_Request_DioSetup_maxAllocation (0)

/** @}
 */

#endif
