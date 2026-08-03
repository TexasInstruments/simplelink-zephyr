/*
 * Copyright (c) 2026 Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BLDRCMDS_H__
#define __BLDRCMDS_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bootloader Command ID Enum
 */
typedef enum {
	BLDR_CMD_ID_PING         = 0,  /**< Ping Command */
	BLDR_CMD_ID_GET_STATUS   = 1,  /**< Get Status Command */
	BLDR_CMD_ID_GET_SEC_VER  = 2,  /**< Get Security Version Command */
	BLDR_CMD_ID_BREAK_OUT    = 3,  /**< Break Out Command */
	BLDR_CMD_ID_RESET        = 4,  /**< Reset Command */
	BLDR_CMD_ID_GET_PART_ID  = 5,  /**< Get Part ID Command */
	BLDR_CMD_ID_GET_KEY_ID   = 6,  /**< Get Customer Key ID Command */
	BLDR_CMD_ID_SET_CUST_KEY = 7,  /**< Set Customer Key Command */
	BLDR_CMD_ID_DOWNLOAD     = 8,  /**< Download Command */
	BLDR_CMD_ID_BANK_ERASE   = 9,  /**< Bank Erase Command */
	BLDR_CMD_ID_SEND_DATA    = 10, /**< Send Data Command */
	BLDR_CMD_ID_GET_FW_HASH  = 11, /**< Get Firmware Hash */
} BLDR_CMD_ID_T;

/**
 * @brief Bootloader Status Enum
 */
typedef enum {
	BLDR_CMD_RET_SUCCESS      = 0x40, /**< Success Status */
	BLDR_CMD_RET_UNKNOWN_CMD  = 0x41, /**< Unknown Command Status */
	BLDR_CMD_RET_INVALID_CMD  = 0x42, /**< Invalid Command Status */
	BLDR_CMD_RET_INVALID_ADR  = 0x43, /**< Invalid Address Status */
	BLDR_CMD_RET_FLASH_FAIL   = 0x44, /**< Flash Failure Status */
	BLDR_CMD_RET_SEC_VER_FAIL = 0x45, /**< Security Version Failure status */
	BLDR_CMD_RET_OUT_OF_ORDER = 0x46, /**< Out of Order Commands status */
	BLDR_CMD_RET_NO_APP       = 0x47, /**< No App (can't breakout) status */
	BLDR_CMD_RET_AUTH_FAIL    = 0x48, /**< Authentication Failure status */
	BLDR_CMD_RET_FAILURE      = 0x49, /**< General Failure status */
} BLDR_STATUS_T;

/*
 * Overhead each and every cmd has:
 * [ length, length ] ... cmd_payload ... [ crc, crc, crc, crc ]
 */
#define BLDR_CMD_OVERHEAD (6U)

/*
 * Constant front overhead of a download cmd - only the content before the
 * variably sized blockHashes:
 * [ imgID, imgID, secVer, size, size, size, size, addr, addr, addr, addr ]
 */
#define BLDR_DOWNLOAD_PAYLOAD_FRONT_OVERHEAD (11U)

/*
 * Constant overhead of a download cmd, excluding the variably sized
 * blockHashes portion:
 * front_overhead + ... block hashes ... + [ ti_signature ] + [ cust_signature ]
 */
#define BLDR_DOWNLOAD_CMD_OVERHEAD (BLDR_DOWNLOAD_PAYLOAD_FRONT_OVERHEAD + 64U + 64U)

/*
 * Constant overhead of a send data cmd, excluding the variably sized block
 * data portion:
 * [ size, size, size, size, num, num, num, num ]
 */
#define BLDR_SEND_DATA_CMD_OVERHEAD (8U)

/* The value that is sent to acknowledge a packet. */
#define BLDR_CMD_ACK (0xCCU)

/* The value that is sent to not-acknowledge a packet. */
#define BLDR_CMD_NAK (0x33U)

#ifdef __cplusplus
}
#endif

#endif /* __BLDRCMDS_H__ */
