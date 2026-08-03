/*
 * Copyright (c) 2026 Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/printk.h>

#include "ssbl_host.h"
#include "bldrcmds.h"

/*
 * Several functions in this file are unused by main.c; they are a resource
 * for use outside of this simple starter example.
 */
#pragma GCC diagnostic ignored "-Wunused-function"

/* SPI bus shared with the ti_cc140xp driver's device and the onboard mx25r80
 * flash - not used concurrently with either in this sample.
 */
static const struct device *const spi0_dev = DEVICE_DT_GET(DT_NODELABEL(spi0));

/*
 * Third pinctrl state (see the board overlay: pinctrl-2/"cs_sw_controlled")
 * remuxing only DIO7 back to plain GPIO while SCK/MOSI/MISO stay on their
 * SPI function. Matches this driver's PINCTRL_STATE_CS_SW_CONTROLLED
 * convention (spi_cc23x0_cc27xx.c): PINCTRL_STATE_PRIV_START + 0 for
 * whichever pinctrl-name occupies that position in the overlay's list.
 */
#define SSBL_PINCTRL_STATE_CS_SW_CONTROLLED (PINCTRL_STATE_PRIV_START + 0)
PINCTRL_DT_DEV_CONFIG_DECLARE(DT_NODELABEL(spi0));
static const struct pinctrl_dev_config *const spi0_pincfg =
	PINCTRL_DT_DEV_CONFIG_GET(DT_NODELABEL(spi0));

/*
 * Reset/trigger GPIOs dedicated to this sample - not shared with the
 * ti_cc140xp driver's own reset/int/poci pins. The trigger pin doubles as
 * SPI CS once spi_transceive() starts being called (see ssbl_spi_cfg below).
 */
static const struct gpio_dt_spec ssbl_reset_gpio =
	GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), reset_gpios);
static const struct gpio_dt_spec ssbl_trigger_gpio =
	GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), trigger_gpios);

/*
 * .cs is left zero-initialized (struct spi_cs_control, not a pointer): the
 * board's default spi0 pinctrl includes CS on DIO7, so the CC27xx SPI driver
 * drives CS itself around every spi_transceive() call.
 */
static const struct spi_config ssbl_spi_cfg = {
	.frequency = 8000000U,
	.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
	.slave = 0,
};

/* The byte sequence that is sent to acknowledge a received packet. */
static const uint8_t g_ui8ACK[2] = {0, BLDR_CMD_ACK};

/**
 * @brief Structure containing security version info
 */
typedef struct {
	uint8_t version; /**< Version of the current running TRX software */
	uint32_t status; /**< Status of the current running TRX software */
	uint8_t slot;     /**< Version slot of the current running TRX software.
			   *   Indicates how many more updates are possible.
			   */
} ssblSecVersionInfo_t;

/**
 * @brief Simple typedef representing a part ID
 */
typedef uint32_t ssblPartId_t;

/**
 * @brief Structure representing a single image block hash
 */
typedef struct {
	uint8_t hash[32]; /**< Hash over a single image block */
} ssblBlockHash_t;

/**
 * @brief All download info which could be required/helpful throughout the update sequence
 *
 * Some of these fields are parsed directly from the TI provided download cmd payload.
 * Others are calculated or inferred based on all information provided.
 *
 * Declared static (not a stack local) - blockHashes alone is
 * TRX_NUM_SECTORS * sizeof(ssblBlockHash_t) = 0x70 * 32 = 3584 bytes, too
 * large to comfortably carry on the call stack.
 */
typedef struct {
	uint16_t imageId;                             /**< Image ID */
	uint8_t secVersion;                           /**< Security version */
	uint32_t size;                                /**< Image size */
	uint32_t remainingSize;                       /**< Remaining size */
	uint32_t startAddress;                        /**< Image start address (where to program on the TRX) */
	uint32_t endAddress;                          /**< Image end address (calculated from start and size) */
	uint8_t numBlocks;                            /**< Number of blocks in the image */
	uint8_t currBlock;                            /**< Current block of the update */
	ssblBlockHash_t blockHashes[TRX_NUM_SECTORS]; /**< Worst-case number of block hashes, based on the
							*   number of sectors on the TRX device
							*/
	ssblEccSignature_t tiSign;                    /**< TI signature */
	ssblEccSignature_t custSign;                  /**< Customer signature */
} ssblDownloadInfo_t;
static ssblDownloadInfo_t g_downloadInfo;

static BLDR_STATUS_T sendPingCommand(void);
static BLDR_STATUS_T sendGetStatusCommand(void);
static BLDR_STATUS_T sendGetSecVerCommand(ssblSecVersionInfo_t *secVersionInfo);
static BLDR_STATUS_T sendBreakOutCommand(bool checkStatus);
static BLDR_STATUS_T sendResetCommand(void);
static BLDR_STATUS_T sendGetPartIdCommand(ssblPartId_t *partId);
static BLDR_STATUS_T sendGetKeyIdCommand(ssblKeyId_t *keyId);
static BLDR_STATUS_T sendGetFwHashCommand(ssblFwHash_t *fwHash);
static BLDR_STATUS_T sendSetCustKeyCommand(const ssblCustKey_t *custKey);
static BLDR_STATUS_T sendDownloadCommand(uint8_t *downloadCmdBuffer, uint16_t buffLength);
static BLDR_STATUS_T sendBankEraseCommand(void);
static BLDR_STATUS_T sendSendDataCommand(uint8_t *blockBuff, uint32_t blockSize, uint32_t blockNum);
static uint32_t swapWord(const uint8_t *pui8Data);
static uint16_t swapShort(const uint8_t *pui8Data);
static bool isMemRangeAllOnes(uint32_t *pMemRangeBase, uint32_t byteCount);

static BLDR_STATUS_T sendPacket(const BLDR_CMD_ID_T cmdId, const uint8_t *payload, const uint16_t payloadSize);
static BLDR_STATUS_T receivePacket(uint8_t *payload, uint16_t *payloadSize);
static BLDR_STATUS_T sendBytes(const uint8_t *payload, const uint16_t payloadSize);
static int ssblSpiWrite(const uint8_t *buf, size_t len);
static int ssblSpiRead(uint8_t *buf, size_t len);
static void ssblResetAndTrigger(bool triggerSsbl);

/*
 * The data buffer assigned for transfers to the SSBL. The payload buffer
 * (g_pui8PayloadBuffer below) starts at offset 2. Sized for the max possible
 * download command: sizeof(max_num_hashes) + BLDR_CMD_OVERHEAD +
 * BLDR_DOWNLOAD_CMD_OVERHEAD.
 */
static uint8_t
	g_pui8DataBuffer[(TRX_NUM_SECTORS * sizeof(ssblBlockHash_t)) + BLDR_CMD_OVERHEAD + BLDR_DOWNLOAD_CMD_OVERHEAD];
static uint8_t *g_pui8PayloadBuffer = &g_pui8DataBuffer[2];

/*
 * All-zero TX source for ssblSpiRead(), sized to cover any read length used
 * in this file - matches TI Drivers' documented NULL-txBuf behavior (drives
 * 0x00 on MOSI during a receive).
 */
static const uint8_t g_zeroFillBuf[sizeof(g_pui8DataBuffer)];

/**
 * Response timeout, in milliseconds. Set once per performSsblDownloadSequence()
 * call (longer if a customer key is in play, to allow for both TI and
 * customer key ECDSA verification on the TRX), reused by every sendPacket()/
 * receivePacket() wait loop for the duration of that call.
 */
static uint32_t g_responseTimeoutMs;

/**
 * @brief Quickly confirm the CRC32 at the end of the trx_fw_update_final.bin in
 * flash matches the stored value
 *
 * @param trxFwUpdateBuffer - address where the trx_fw_update_final.bin has been
 * stored in flash
 * @return BLDR_STATUS_T
 */
BLDR_STATUS_T crcCheckTrxFwUpdateBuffer(const uint8_t *trxFwUpdateBuffer)
{
	uint32_t imageSize, downloadPayloadSize, totalSize;
	uint8_t numBlocks;
	uint32_t calcCrc, storedCrc;

	imageSize = swapWord(&trxFwUpdateBuffer[3]);
	numBlocks = (imageSize + (TRX_MAIN_FLASH_SECTOR_SIZE - 1)) / TRX_MAIN_FLASH_SECTOR_SIZE;
	downloadPayloadSize = BLDR_DOWNLOAD_CMD_OVERHEAD + (sizeof(ssblBlockHash_t) * numBlocks);
	totalSize = imageSize + downloadPayloadSize;

	/* Sanity check against the max size the update buffer/protocol can hold */
	if ((totalSize > (TRX_NUM_SECTORS * TRX_MAIN_FLASH_SECTOR_SIZE)) ||
	    (imageSize >= (TRX_NUM_SECTORS * TRX_MAIN_FLASH_SECTOR_SIZE))) {
		return BLDR_CMD_RET_FAILURE;
	}

	calcCrc = crc32_ieee((uint8_t *)trxFwUpdateBuffer, totalSize);
	storedCrc = swapWord(&trxFwUpdateBuffer[totalSize]);
	if (calcCrc != storedCrc) {
		return BLDR_CMD_RET_FAILURE;
	}

	return BLDR_CMD_RET_SUCCESS;
}

/**
 * @brief Hold the TRX in reset, apply (or release) the SSBL trigger, then
 * release reset and let the SSBL detect the trigger level.
 *
 * @param triggerSsbl true to activate the SSBL via the trigger pin
 */
static void ssblResetAndTrigger(bool triggerSsbl)
{
	/*
	 * The CC27xx SPI driver applies SCK/MOSI/MISO/CSN pinctrl lazily on
	 * the first spi_transceive() call, producing a one-time clock glitch
	 * during that pin-mux transition. Absorb it here, before DIO7 is
	 * reconfigured as plain GPIO below.
	 */
	static bool spiPinctrlPrimed;

	if (!spiPinctrlPrimed) {
		uint8_t dummy = 0x00U;
		struct spi_buf primeBuf = {.buf = &dummy, .len = sizeof(dummy)};
		struct spi_buf_set primeBufs = {.buffers = &primeBuf, .count = 1U};

		spi_transceive(spi0_dev, &ssbl_spi_cfg, &primeBufs, NULL);
		spiPinctrlPrimed = true;
	}

	/*
	 * Remux DIO7 (CSN) back to plain GPIO so it can be driven directly
	 * below; SCK/MOSI/MISO stay on their SPI function.
	 */
	pinctrl_apply_state(spi0_pincfg, SSBL_PINCTRL_STATE_CS_SW_CONTROLLED);

	/* Configure both pins as outputs; idempotent, safe to call every time */
	gpio_pin_configure_dt(&ssbl_reset_gpio, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&ssbl_trigger_gpio, GPIO_OUTPUT_INACTIVE);

	/* Hold TRX in reset */
	gpio_pin_set_dt(&ssbl_reset_gpio, 1);
	k_busy_wait(50);

	/*
	 * gpio_pin_set_dt() already applies the GPIO_ACTIVE_LOW inversion from
	 * trigger-gpios, so passing triggerSsbl directly (not negated) yields
	 * the correct physical LOW-when-true SSBL entry level.
	 */
	gpio_pin_set_dt(&ssbl_trigger_gpio, triggerSsbl);
	k_busy_wait(50);

	/* Release TRX from reset and allow the SSBL time to detect the trigger */
	gpio_pin_set_dt(&ssbl_reset_gpio, 0);
	k_busy_wait(400);

	/* Force CSN/trigger inactive to avoid additional SPI glitches */
	gpio_pin_set_dt(&ssbl_trigger_gpio, 0);

	/*
	 * Hand DIO7 back to the SPI0 peripheral as hardware CSN before the
	 * next transfer; the driver won't re-apply pinctrl on its own since
	 * ssbl_spi_cfg is unchanged from the priming transfer above.
	 */
	pinctrl_apply_state(spi0_pincfg, PINCTRL_STATE_DEFAULT);
}

/**
 * @brief Performs all steps required to complete a software update
 *
 * @param downloadStruct
 * @return BLDR_STATUS_T
 */
BLDR_STATUS_T performSsblDownloadSequence(const ssblDownload_t *downloadStruct)
{
	BLDR_STATUS_T status = BLDR_CMD_RET_FAILURE;
	uint32_t downloadPayloadSize;

	/* If programKey is true, custKey must be provided */
	if ((downloadStruct->programKey && !downloadStruct->custKey) || (!downloadStruct->trxFwUpdateBuffer)) {
		return (BLDR_STATUS_T)BLDR_CMD_RET_NO_CUST_KEY;
	}

	/* Confirm there is a potentially valid image in flash */
	if (isMemRangeAllOnes((uint32_t *)downloadStruct->trxFwUpdateBuffer, 8)) {
		return (BLDR_STATUS_T)BLDR_CMD_RET_NO_IMAGE_FOUND;
	}

	/* Quick CRC check over the TRX FW image before doing anything else */
	if (BLDR_CMD_RET_FAILURE == crcCheckTrxFwUpdateBuffer((uint8_t *)downloadStruct->trxFwUpdateBuffer)) {
		return (BLDR_STATUS_T)BLDR_CMD_RET_CRC_VERIFY_FAILED;
	}

	ssblResetAndTrigger(downloadStruct->triggerSsbl);

	/*
	 * Allow extra time if a customer key signature must also be verified
	 * on the TRX, on top of the TI signature.
	 */
	g_responseTimeoutMs = downloadStruct->custKey ? 4000U : 2500U;

	/* Parse downloadCmd to get helpful information out of it */
	g_downloadInfo.imageId = swapShort(&downloadStruct->trxFwUpdateBuffer[0]);
	g_downloadInfo.secVersion = downloadStruct->trxFwUpdateBuffer[2];
	g_downloadInfo.size = swapWord(&downloadStruct->trxFwUpdateBuffer[3]);
	g_downloadInfo.startAddress = swapWord(&downloadStruct->trxFwUpdateBuffer[7]);

	/* Calculate some helpful values to save for later use (round up numBlocks) */
	g_downloadInfo.endAddress = g_downloadInfo.startAddress + g_downloadInfo.size;
	g_downloadInfo.numBlocks = (g_downloadInfo.size + (TRX_MAIN_FLASH_SECTOR_SIZE - 1)) / TRX_MAIN_FLASH_SECTOR_SIZE;
	downloadPayloadSize = BLDR_DOWNLOAD_CMD_OVERHEAD + (sizeof(ssblBlockHash_t) * g_downloadInfo.numBlocks);

	/* Verify the host has a line of communication with the SSBL */
	status = sendPingCommand();
	if (BLDR_CMD_RET_SUCCESS != status) {
		HANDLE_FAIL();
	}

	/* Verify security version is as expected (downgrade protection) */
	ssblSecVersionInfo_t initialSecVersionInfo;

	status = sendGetSecVerCommand(&initialSecVersionInfo);
	if ((BLDR_CMD_RET_SUCCESS != status) || (initialSecVersionInfo.version > g_downloadInfo.secVersion)) {
		HANDLE_FAIL();
	}

	if (downloadStruct->programKey) {
		/* Set the customer key (one-time-only operation) */
		status = sendSetCustKeyCommand(downloadStruct->custKey);
		if (BLDR_CMD_RET_SUCCESS != status) {
			HANDLE_FAIL();
		}
	}

	if (downloadStruct->custKey) {
		/* Verify the provided key was written successfully */
		ssblKeyId_t newKeyId;

		status = sendGetKeyIdCommand(&newKeyId);
		if ((BLDR_CMD_RET_SUCCESS != status) ||
		    (memcmp(downloadStruct->custKey->keyId, newKeyId, sizeof(newKeyId)))) {
			HANDLE_FAIL();
		}
	}

	/*
	 * The order of these three commands is enforced: DOWNLOAD -> BANK_ERASE
	 * -> SEND_DATA[N]. Sending them out of order returns a failure response.
	 */
	status = sendDownloadCommand(downloadStruct->trxFwUpdateBuffer, downloadPayloadSize);
	if (BLDR_CMD_RET_SUCCESS != status) {
		HANDLE_FAIL();
	}

	status = sendBankEraseCommand();
	if (BLDR_CMD_RET_SUCCESS != status) {
		HANDLE_FAIL();
	}

	/* Send each new image block one by one */
	g_downloadInfo.remainingSize = g_downloadInfo.size;
	for (g_downloadInfo.currBlock = 0; g_downloadInfo.currBlock < g_downloadInfo.numBlocks;
	     g_downloadInfo.currBlock++) {
		uint32_t blockSize = TRX_MAIN_FLASH_SECTOR_SIZE;
		uint32_t blockOffset = (blockSize * g_downloadInfo.currBlock) + downloadPayloadSize;

		if (g_downloadInfo.currBlock == (g_downloadInfo.numBlocks - 1)) {
			blockSize = g_downloadInfo.remainingSize;
		}

		status = sendSendDataCommand(&downloadStruct->trxFwUpdateBuffer[blockOffset], blockSize,
					      g_downloadInfo.currBlock);
		if (BLDR_CMD_RET_SUCCESS != status) {
			HANDLE_FAIL();
		}

		g_downloadInfo.remainingSize -= blockSize;
	}

	/* Verify the download succeeded by checking the new security version info */
	ssblSecVersionInfo_t newSecVersionInfo;

	status = sendGetSecVerCommand(&newSecVersionInfo);
	if ((BLDR_CMD_RET_SUCCESS != status) || (newSecVersionInfo.status != 0xAAAAAAAA) ||
	    (newSecVersionInfo.version != g_downloadInfo.secVersion)) {
		HANDLE_FAIL();
	}

	/* Get and display the new FW hash */
	ssblFwHash_t currFwHash;

	status = sendGetFwHashCommand(&currFwHash);
	if (BLDR_CMD_RET_SUCCESS != status) {
		HANDLE_FAIL();
	}
	printk("Current FW Hash = 0x%x%x%x%x%x%x%x%x\n",
	       currFwHash[0], currFwHash[1], currFwHash[2], currFwHash[3],
	       currFwHash[4], currFwHash[5], currFwHash[6], currFwHash[7]);

	/*
	 * Don't attempt to send a GetStatus cmd - the device isn't expected to
	 * respond. status here only reflects whether the cmd itself was ACKed.
	 */
	status = sendBreakOutCommand(false);
	if (BLDR_CMD_RET_SUCCESS != status) {
		HANDLE_FAIL();
	}

	return status;
}

/**
 * @brief Send a Ping Command
 *
 * Status, in this case is only if an ACK was received.
 *
 * @return BLDR_STATUS_T
 */
static BLDR_STATUS_T sendPingCommand(void)
{
	return sendPacket(BLDR_CMD_ID_PING, NULL, 0);
}

/**
 * @brief Send a Get Status Command and return the result
 *
 * @return BLDR_STATUS_T
 */
static BLDR_STATUS_T sendGetStatusCommand(void)
{
	BLDR_STATUS_T status = BLDR_CMD_RET_FAILURE;

	if (BLDR_CMD_RET_SUCCESS == sendPacket(BLDR_CMD_ID_GET_STATUS, NULL, 0)) {
		uint16_t rxLength = 1;

		if (BLDR_CMD_RET_SUCCESS == (status = receivePacket(g_pui8DataBuffer, &rxLength))) {
			sendBytes((const uint8_t *)g_ui8ACK, 2);
			status = (BLDR_STATUS_T)g_pui8PayloadBuffer[0];
		}
	}

	return status;
}

/**
 * @brief Send a Get Security Version Command and return the status
 *
 * If successful, the retrieved security version information will reside in the
 * secVersionInfo parameter.
 *
 * @param secVersionInfo
 * @return BLDR_STATUS_T
 */
static BLDR_STATUS_T sendGetSecVerCommand(ssblSecVersionInfo_t *secVersionInfo)
{
	BLDR_STATUS_T status = BLDR_CMD_RET_FAILURE;

	if (BLDR_CMD_RET_SUCCESS == (status = sendPacket(BLDR_CMD_ID_GET_SEC_VER, NULL, 0))) {
		uint16_t rxLength = 5;

		if (BLDR_CMD_RET_SUCCESS == (status = receivePacket(g_pui8DataBuffer, &rxLength))) {
			sendBytes((uint8_t *)g_ui8ACK, 2);

			secVersionInfo->version = g_pui8PayloadBuffer[0];
			uint8_t *statusBuff = (uint8_t *)&secVersionInfo->status;
			statusBuff[0] = g_pui8PayloadBuffer[1];
			statusBuff[1] = g_pui8PayloadBuffer[2];
			statusBuff[2] = g_pui8PayloadBuffer[3];
			statusBuff[3] = g_pui8PayloadBuffer[4];
			secVersionInfo->slot = g_pui8PayloadBuffer[5];

			status = sendGetStatusCommand();
		}
	}

	return status;
}

/**
 * @brief Send a Break Out Command
 *
 * This function will only check the status of the break out command if requested
 * via the checkStatus parameter.
 *
 * This is useful because if the break out command was to succeed, the TRX device
 * will no longer be running the SSBL software. Which means any attempt to get
 * the status would not be understood.
 *
 * @param checkStatus
 * @return BLDR_STATUS_T
 */
static BLDR_STATUS_T sendBreakOutCommand(bool checkStatus)
{
	BLDR_STATUS_T status = BLDR_CMD_RET_FAILURE;

	if (BLDR_CMD_RET_SUCCESS == (status = sendPacket(BLDR_CMD_ID_BREAK_OUT, NULL, 0))) {
		if (checkStatus) {
			status = sendGetStatusCommand();
		}
	}

	return status;
}

/**
 * @brief Send a Reset Command
 *
 * @return BLDR_STATUS_T
 */
static BLDR_STATUS_T sendResetCommand(void)
{
	return sendPacket(BLDR_CMD_ID_RESET, NULL, 0);
}

/**
 * @brief Send a Get Part ID Command and return the status
 *
 * If successful, the retrieved part id will reside in the partId parameter.
 *
 * @param partId
 * @return BLDR_STATUS_T
 */
static BLDR_STATUS_T sendGetPartIdCommand(ssblPartId_t *partId)
{
	BLDR_STATUS_T status = BLDR_CMD_RET_FAILURE;

	if (BLDR_CMD_RET_SUCCESS == (status = sendPacket(BLDR_CMD_ID_GET_PART_ID, NULL, 0))) {
		uint16_t rxLength = sizeof(ssblPartId_t);

		if (BLDR_CMD_RET_SUCCESS == (status = receivePacket(g_pui8DataBuffer, &rxLength))) {
			sendBytes((uint8_t *)g_ui8ACK, 2);

			((uint8_t *)partId)[0] = g_pui8PayloadBuffer[3];
			((uint8_t *)partId)[1] = g_pui8PayloadBuffer[2];
			((uint8_t *)partId)[2] = g_pui8PayloadBuffer[1];
			((uint8_t *)partId)[3] = g_pui8PayloadBuffer[0];

			status = sendGetStatusCommand();
		}
	}

	return status;
}

/**
 * @brief Send a Get Key ID Command and return the status
 *
 * If successful, the retrieved key ID will reside in the keyId parameter
 *
 * @param keyId
 * @return BLDR_STATUS_T
 */
static BLDR_STATUS_T sendGetKeyIdCommand(ssblKeyId_t *keyId)
{
	BLDR_STATUS_T status = BLDR_CMD_RET_FAILURE;

	if (BLDR_CMD_RET_SUCCESS == (status = sendPacket(BLDR_CMD_ID_GET_KEY_ID, NULL, 0))) {
		uint16_t rxLength = sizeof(ssblKeyId_t);

		if (BLDR_CMD_RET_SUCCESS == (status = receivePacket(g_pui8DataBuffer, &rxLength))) {
			sendBytes((uint8_t *)g_ui8ACK, 2);
			memcpy((uint8_t *)keyId, g_pui8PayloadBuffer, sizeof(ssblKeyId_t));
			status = sendGetStatusCommand();
		}
	}

	return status;
}

/**
 * @brief Send a Get FW Hash ID Command and return the status
 *
 * If successful, the retrieved FW Hash will reside in the fwHash parameter
 *
 * @param fwHash
 * @return BLDR_STATUS_T
 */
static BLDR_STATUS_T sendGetFwHashCommand(ssblFwHash_t *fwHash)
{
	BLDR_STATUS_T status = BLDR_CMD_RET_FAILURE;

	if (BLDR_CMD_RET_SUCCESS == (status = sendPacket(BLDR_CMD_ID_GET_FW_HASH, NULL, 0))) {
		uint16_t rxLength = sizeof(ssblFwHash_t);

		if (BLDR_CMD_RET_SUCCESS == (status = receivePacket(g_pui8DataBuffer, &rxLength))) {
			sendBytes((uint8_t *)g_ui8ACK, 2);
			memcpy((uint8_t *)fwHash, g_pui8PayloadBuffer, sizeof(ssblFwHash_t));
			status = sendGetStatusCommand();
		}
	}

	return status;
}

/**
 * @brief Send a Set Customer Key Command and return the status
 *
 * @param custKey
 * @return BLDR_STATUS_T
 */
static BLDR_STATUS_T sendSetCustKeyCommand(const ssblCustKey_t *custKey)
{
	BLDR_STATUS_T status = BLDR_CMD_RET_FAILURE;

	if (BLDR_CMD_RET_SUCCESS ==
	    (status = sendPacket(BLDR_CMD_ID_SET_CUST_KEY, (uint8_t *)custKey, sizeof(ssblCustKey_t)))) {
		status = sendGetStatusCommand();
	}

	return status;
}

/**
 * @brief Send a Download Command and return the status
 *
 * @param downloadCmdBuffer
 * @param buffLength
 * @return BLDR_STATUS_T
 */
static BLDR_STATUS_T sendDownloadCommand(uint8_t *downloadCmdBuffer, uint16_t buffLength)
{
	BLDR_STATUS_T status = BLDR_CMD_RET_FAILURE;

	if (BLDR_CMD_RET_SUCCESS ==
	    (status = sendPacket(BLDR_CMD_ID_DOWNLOAD, (uint8_t *)downloadCmdBuffer, buffLength))) {
		status = sendGetStatusCommand();
	}

	return status;
}

/**
 * @brief Send a Bank Erase Command and return the status
 *
 * @return BLDR_STATUS_T
 */
static BLDR_STATUS_T sendBankEraseCommand(void)
{
	BLDR_STATUS_T status = BLDR_CMD_RET_FAILURE;

	if (BLDR_CMD_RET_SUCCESS == (status = sendPacket(BLDR_CMD_ID_BANK_ERASE, NULL, 0))) {
		status = sendGetStatusCommand();
	}
	return status;
}

/**
 * @brief Send a Send Data Command and return the status
 *
 * @param blockBuff
 * @param blockSize
 * @param blockNum
 * @return BLDR_STATUS_T
 */
static BLDR_STATUS_T sendSendDataCommand(uint8_t *blockBuff, uint32_t blockSize, uint32_t blockNum)
{
	BLDR_STATUS_T status = BLDR_CMD_RET_FAILURE;
	uint8_t *blockSizePtr = (uint8_t *)&blockSize;
	uint8_t *blockNumPtr = (uint8_t *)&blockNum;

	g_pui8PayloadBuffer[1] = blockSizePtr[3];
	g_pui8PayloadBuffer[2] = blockSizePtr[2];
	g_pui8PayloadBuffer[3] = blockSizePtr[1];
	g_pui8PayloadBuffer[4] = blockSizePtr[0];
	g_pui8PayloadBuffer[5] = blockNumPtr[3];
	g_pui8PayloadBuffer[6] = blockNumPtr[2];
	g_pui8PayloadBuffer[7] = blockNumPtr[1];
	g_pui8PayloadBuffer[8] = blockNumPtr[0];
	memcpy(&g_pui8PayloadBuffer[9], blockBuff, blockSize);

	/* NULL payload with a non-NULL size indicates the payload is pre-prepared */
	if (BLDR_CMD_RET_SUCCESS == (status = sendPacket(BLDR_CMD_ID_SEND_DATA, NULL, blockSize + 8))) {
		status = sendGetStatusCommand();
	}

	return status;
}

/**
 * @brief Perform a blocking SPI write.
 *
 * CS is not toggled manually here - the board's default spi0 pinctrl (see
 * the overlay) puts DIO7 under the CC27xx SPI driver's own hardware CS
 * control, which asserts/deasserts it automatically around this
 * spi_transceive() call.
 */
static int ssblSpiWrite(const uint8_t *buf, size_t len)
{
	struct spi_buf tx_buf = {.buf = (void *)buf, .len = len};
	struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1U};
	static bool errorPrinted;
	int ret;

	ret = spi_transceive(spi0_dev, &ssbl_spi_cfg, &tx_bufs, NULL);

	if ((ret != 0) && !errorPrinted) {
		printk("ssblSpiWrite: spi_transceive returned %d\n", ret);
		errorPrinted = true;
	}

	return ret;
}

/**
 * @brief Perform a blocking SPI read.
 *
 * CS is not toggled manually here - see ssblSpiWrite(). Drives an explicit
 * all-zero TX buffer (g_zeroFillBuf) instead of passing NULL for tx_bufs, so
 * PICO reads 0x00 during this transfer instead of this SPI driver's own
 * 0xFF dummy-fill default.
 */
static int ssblSpiRead(uint8_t *buf, size_t len)
{
	struct spi_buf tx_buf = {.buf = (void *)g_zeroFillBuf, .len = len};
	struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1U};
	struct spi_buf rx_buf = {.buf = buf, .len = len};
	struct spi_buf_set rx_bufs = {.buffers = &rx_buf, .count = 1U};
	static bool errorPrinted;
	int ret;

	ret = spi_transceive(spi0_dev, &ssbl_spi_cfg, &tx_bufs, &rx_bufs);

	if ((ret != 0) && !errorPrinted) {
		printk("ssblSpiRead: spi_transceive returned %d\n", ret);
		errorPrinted = true;
	}

	return ret;
}

/**
 * @brief Send a payload in packet form
 *
 * cmdID is the only required param. If either payload or payloadSize are
 * zero then only the cmdID will be sent to the lower level packet handler.
 *
 * In consideration of the SEND_DATA command, if a payloadSize is provided but
 * not a payload. It is assumed that the payload has been copied into the
 * g_pui8PayloadBuffer already and the memcpy operation is skipped.
 *
 * @param cmdId
 * @param payload
 * @param payloadSize
 * @return BLDR_STATUS_T
 */
static BLDR_STATUS_T sendPacket(const BLDR_CMD_ID_T cmdId, const uint8_t *payload, const uint16_t payloadSize)
{
	uint16_t finalSize = payloadSize;
	uint32_t ui32Crc, swappedCrc;

	g_pui8PayloadBuffer[0] = cmdId;
	/* Special case for the SendData cmd: the payload buffer is already filled */
	if (payload && payloadSize) {
		memcpy(&g_pui8PayloadBuffer[1], payload, payloadSize);
	}

	/* Include the size, checksum and cmdID bytes in the total size */
	finalSize += (sizeof(finalSize) + sizeof(ui32Crc) + 1);
	memcpy((void *)g_pui8DataBuffer, (void *)&finalSize, sizeof(finalSize));

	/* Calculate the checksum to be sent out with the data and add it to the buffer */
	ui32Crc = crc32_ieee((uint8_t *)g_pui8DataBuffer, finalSize - sizeof(ui32Crc));
	swappedCrc = swapWord((uint8_t *)&ui32Crc);
	memcpy((void *)&g_pui8DataBuffer[finalSize - sizeof(ui32Crc)], (void *)&swappedCrc, sizeof(swappedCrc));

	ssblSpiWrite(g_pui8DataBuffer, finalSize);

	/* Wait for a non-zero byte response */
	uint8_t ui8Rsp = 0;
	int64_t deadline = k_uptime_get() + g_responseTimeoutMs;

	while ((ui8Rsp == 0) && (k_uptime_get() < deadline)) {
		ssblSpiRead(&ui8Rsp, 1);
	}

	if (ui8Rsp == 0) {
		/*
		 * All other error codes are relative to the specific SSBL CMD. This
		 * timeout check however is not a SSBL CMD and thus there is not a
		 * BLDR_STATUS_T for it.
		 */
		return (BLDR_STATUS_T)BLDR_CMD_RET_RESP_TIMEOUT;
	}

	if (ui8Rsp != BLDR_CMD_ACK) {
		/*
		 * Diagnostic only: BLDR_CMD_RET_FAILURE alone doesn't distinguish a
		 * real NAK (0x33) from SPI-level garbage. 0x33 points at the SSBL
		 * rejecting our packet (likely CRC-32 mismatch); anything else
		 * points at a framing/timing problem on the SPI transfer itself.
		 */
		printk("sendPacket: cmd 0x%x got non-ACK response byte 0x%02x\n", cmdId, ui8Rsp);
		return BLDR_CMD_RET_FAILURE;
	}

	return BLDR_CMD_RET_SUCCESS;
}

/**
 * @brief Send a payload not in packet form
 *
 * Sends the payloadSize bytes of the provided payload buffer without first
 * formatting it into a packet.
 *
 * @param payload
 * @param payloadSize
 * @return BLDR_STATUS_T
 */
static BLDR_STATUS_T sendBytes(const uint8_t *payload, uint16_t payloadSize)
{
	ssblSpiWrite(payload, payloadSize);

	return BLDR_CMD_RET_SUCCESS;
}

/**
 * @brief Receive a packet
 *
 * Receives a packet from the SSBL on the CC140X. The will populate the payload
 * and payloadSize pointers based on the content received.
 *
 * ** Warning ** this function does not time out on the CRC and payload phase.
 * It only enforces the response timeout while waiting for the first non-zero
 * byte.
 *
 * @param payload
 * @param payloadSize
 * @return BLDR_STATUS_T
 */
static BLDR_STATUS_T receivePacket(uint8_t *payload, uint16_t *payloadSize)
{
	uint32_t ui32Crc;
	uint16_t ui16Size;
	uint16_t ui16PayloadSize;
	uint8_t tempBytes[4];

	/*
	 * Wait for a non-zero value. Because it is guaranteed to be non-zero,
	 * the first value received is the first byte of the 'packet size'
	 * field, in little endian.
	 */
	int64_t deadline = k_uptime_get() + g_responseTimeoutMs;

	do {
		ssblSpiRead(payload, 1);
	} while ((payload[0] == 0) && (k_uptime_get() < deadline));

	if (payload[0] == 0) {
		/*
		 * All other error codes are relative to the specific SSBL CMD. This
		 * timeout check however is not a SSBL CMD and thus there is not a
		 * BLDR_STATUS_T for it.
		 */
		return (BLDR_STATUS_T)BLDR_CMD_RET_RESP_TIMEOUT;
	}

	/*
	 * Now that the first non-zero byte has been received, read the second
	 * byte of the 'packet size' field and receive the rest of the packet.
	 */
	ssblSpiRead(&payload[1], 1);
	ui16Size = (*(uint16_t *)payload);

	/* Receive the payload - used for cmd processing */
	ui16PayloadSize = ui16Size - BLDR_CMD_OVERHEAD;
	ssblSpiRead(&payload[sizeof(ui16Size)], ui16PayloadSize);

	/* Receive the CRC of the packet (remaining sizeof(ui32Crc) bytes) */
	ssblSpiRead(tempBytes, sizeof(ui32Crc));
	ui32Crc = swapWord(tempBytes);

	/* Calculate the CRC of the packet and compare against the received CRC */
	if (crc32_ieee(payload, ui16Size - sizeof(ui32Crc)) != ui32Crc) {
		return BLDR_CMD_RET_FAILURE;
	}

	*payloadSize = ui16PayloadSize;

	return BLDR_CMD_RET_SUCCESS;
}

/**
 * @brief Swap a series of 4 bytes from a buffer and return a single 32-bit word
 *
 * @param pui8Data
 * @return swapped_word
 */
static uint32_t swapWord(const uint8_t *pui8Data)
{
	return ((pui8Data[0] << 24) | (pui8Data[1] << 16) | (pui8Data[2] << 8) | (pui8Data[3]));
}

/**
 * @brief Swap 2 bytes from a buffer and return a single 16-bit short
 *
 * @param pui8Data
 * @return swapped_short
 */
static uint16_t swapShort(const uint8_t *pui8Data)
{
	return ((pui8Data[0] << 8) | pui8Data[1]);
}

/**
 * @brief Check if a memory range is all ones (erased)
 *
 * @param pMemRangeBase    Pointer to start of memory range (will return false if not a multiple of 4)
 * @param byteCount        Number of bytes in the range (will return false if not a multiple of 4)
 * @return true if memory is all ones, else false
 */
static bool isMemRangeAllOnes(uint32_t *pMemRangeBase, uint32_t byteCount)
{
	if ((uint32_t)pMemRangeBase & (uint32_t)0x3 || (uint32_t)byteCount & (uint32_t)0x3) {
		return false;
	}

	uint32_t wordComb = 0xFFFFFFFF;
	uint32_t *pWord = pMemRangeBase;
	uint32_t *pEnd = (uint32_t *)((uint32_t)pMemRangeBase + byteCount);

	while (pWord < pEnd) {
		wordComb &= *(pWord++);
	}
	return (wordComb == 0xFFFFFFFF);
}
