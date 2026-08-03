/*
 * Copyright (c) 2026 Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SSBL_HOST
#define SSBL_HOST

#ifdef __cplusplus
extern "C" {
#endif

#include "bldrcmds.h"

#define TRX_MAIN_FLASH_SECTOR_SIZE 0x400 /**< Flash sector size of the TRX device */
#define TRX_NUM_SECTORS            0x70  /**< Max num of flash sectors for a TRX FW upgrade across all devices */

#define SEC_VER_STATUS_INACTIVE    0xFFFFFFFF /**< Security version status Inactive */
#define SEC_VER_STATUS_ACTIVE      0xAAAAAAAA /**< Security version status Active */
#define SEC_VER_STATUS_OUT_OF_DATE 0x00000000 /**< Security version out of date */

#define SEC_VER_MAX_SLOTS 40 /**< The max number of security version updates */

#define TRX_SSBL_TRIGGER_LEVEL 0 /**< The TRX SSBL is triggered when its trigger pin is active low on bootup */

/**
 * @brief TRAP_FAILURE macro used for changing the behavior of a host encountered error during runtime
 *
 * If you would like to debug an issue it may be easier to enable TRAP_FAILURE so the PC stays where the
 * error occurred. No SPI_close()/Timer_close() call is needed here, since the SPI bus is a static
 * DEVICE_DT_GET reference (not opened/closed) and there is no hardware timer handle.
 */
#ifdef TRAP_FAILURE
/* Loop forever so the error can be debugged in place */
#define HANDLE_FAIL() while (1) {}
#else
/* Return immediately with the failing status */
#define HANDLE_FAIL() return status
#endif

/**
 * @brief Extended Bootloader status Enums
 * These enums extend the BLDR_STATUS_T enums that are defined
 * in bldrcmds.h. The bldrcmds.h file is a shared file and should
 * not contain these host specific enums. Thus the extension.
 *
 * Take special care that these enums do not overlap with the
 * originals from bldrcmds.h.
 */
typedef enum {
	BLDR_CMD_RET_NO_IMAGE_FOUND    = 0x01, /**< First 8 bytes of image were all 1s */
	BLDR_CMD_RET_NO_CUST_KEY       = 0x02, /**< Customer key not provided as expected */
	BLDR_CMD_RET_CRC_VERIFY_FAILED = 0x03, /**< Calculated CRC did not match provided value */
	BLDR_CMD_RET_RESP_TIMEOUT      = 0x04, /**< No response received from the TRX within (t) time */
} BLDR_STATUS_EXT_T;

/**
 * @brief Simple 8 byte buffer which represents a Key ID
 */
typedef uint8_t ssblKeyId_t[8];

/**
 * @brief Simple 256 byte buffer which represents a FW Hash
 */
typedef uint32_t ssblFwHash_t[8];

/**
 * @brief A structure which splits a public key into its components
 */
typedef union {
	uint8_t val8[65]; /**< Byte array used to reference the entire struct at once in code */
	struct {
		uint8_t prefix; /**< Key prefix. Only uncompressed keys are supported (0x04) */
		uint8_t r[32];  /**< Key R component (also known as the X component) */
		uint8_t s[32];  /**< Key S component (also known as the Y component) */
	};
} ssblEccSignature_t;

/**
 * @brief A structure containing a #keyId and a #key.
 */
typedef struct {
	ssblKeyId_t keyId;      /**< Key ID - used to identify and verify which key was written to a given device */
	ssblEccSignature_t key; /**< Key - actual public key contents */
} ssblCustKey_t;

/**
 * @brief A structure containing all required parameters to the performSsblDownloadSequence() function.
 */
typedef struct {
	const ssblCustKey_t *custKey; /**< Pointer to an optional customer public struct */
	bool programKey;              /**< Tells the host to program the provided #custKey */
	uint8_t *trxFwUpdateBuffer;   /**< Byte pointer to the trx_fw_update_final.bin that should reside
					*   somewhere in flash (must be a multiple of 4)
					*/
	bool triggerSsbl;             /**< Tells the host to trigger the SSBL via the trigger pin */
} ssblDownload_t;

/**
 * @brief Quickly confirm the CRC32 at the end of the trx_fw_update_final.bin in
 * flash matches the stored value
 *
 * @param trxFwUpdateBuffer - address where the trx_fw_update_final.bin has been
 * stored in flash
 * @return BLDR_STATUS_T
 */
BLDR_STATUS_T crcCheckTrxFwUpdateBuffer(const uint8_t *trxFwUpdateBuffer);

/**
 * @brief Performs all steps required to complete a software update
 *
 * This function will make decisions based on the information provided in the
 * downloadStruct.
 *
 * It is a failure condition if downloadStruct.programKey is true but the
 * downloadStruct.custKey is NULL. Additionally, it is required that both downloadStruct.downloadCmdPayload and
 * downloadStruct.newImageAddress are not NULL.
 *
 * The overall return behavior of this function can be modified by defining `TRAP_FAILURE`.
 *
 * This function will send the following basic commands:
 *      1. Ping - Verifying a line of communication
 *      2. Get Sec Version - Verify the new image can be accepted
 *      3. Set Customer Key - If requested by downloadStruct.programKey
 *      4. Get Key Id -  If downloadStruct.custKey is provided, confirm keyIds match
 *      5. Download - Start the actual Download
 *      6. Bank Erase - Clear the TRX Flash and prepare for new software
 *      7. Send Data - Transfer N blocks until update is complete
 *      8. Get Sec Version - Verify the new image version is reported after update
 *      9. Break Out - Cause TRX to boot into the new software
 *
 * @param downloadStruct
 * @return BLDR_STATUS_T
 */
BLDR_STATUS_T performSsblDownloadSequence(const ssblDownload_t *downloadStruct);

#ifdef __cplusplus
}
#endif

#endif /* SSBL_HOST */
