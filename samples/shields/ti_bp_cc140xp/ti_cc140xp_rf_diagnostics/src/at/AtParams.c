/*
 * Copyright (c) 2026 Texas Instruments Incorporated
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr adaptation: removes DeviceFamily_constructPath includes (not used in body).
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <TestMode.h>

#include "at/AtParams.h"
#include "at/AtProcess.h"
#include "at/DbgPrint.h"
#include "at/AtTerm.h"
#include "radio/radio.h"

uint8_t AtParams_echoEnabled = 1;

static uint32_t perNumPkts = NUMBER_OF_PACKETS;
static uint32_t perPktLen = PACKET_LENGTH;
static uint16_t packetTxCount = 0;
RF_Frequency frequency = {
    .freq = 0,
    .mdrFreq = 0
};

static AtProcess_Status atParamReadPerNumPackets(char *paramStr, uint32_t paramLen);
static AtProcess_Status atParamWritePerNumPackets(char *paramStr, uint32_t paramLen);
static AtProcess_Status atParamReadPerPacketLen(char *paramStr, uint32_t paramLen);
static AtProcess_Status atParamWritePerPacketLen(char *paramStr, uint32_t paramLen);
static AtProcess_Status atParamReadTestMode(char *paramStr, uint32_t paramLen);
static AtProcess_Status atParamWriteTestMode(char *paramStr, uint32_t paramLen);
static AtProcess_Status atParamWriteAtEcho(char *paramStr, uint32_t paramLen);
static AtProcess_Status atParamReadAtEcho(char *paramStr, uint32_t paramLen);
static AtProcess_Status atParamReadFreq(char *paramStr, uint32_t paramLen);
static AtProcess_Status atParamWriteFreq(char *paramStr, uint32_t paramLen);
static AtProcess_Status atParamReadPower(char *paramStr, uint32_t paramLen);
static AtProcess_Status atParamWritePower(char *paramStr, uint32_t paramLen);
static AtProcess_Status atParamReadCurrRssi(char *paramStr, uint32_t paramLen);
static AtProcess_Status atParamReadAvgRssi(char *paramStr, uint32_t paramLen);
static AtProcess_Status atParamReadMinRssi(char *paramStr, uint32_t paramLen);
static AtProcess_Status atParamReadMaxRssi(char *paramStr, uint32_t paramLen);
static AtProcess_Status atParamReadPHyNames(char *paramStr, uint32_t paramLen);
static AtProcess_Status atParamReadRadioVersion(char *paramStr, uint32_t paramLen);

AtCommand_t atParamsters[] =
{
    {"PP?", atParamReadPerNumPackets},
    {"pp?", atParamReadPerNumPackets},
    {"PP=", atParamWritePerNumPackets},
    {"pp=", atParamWritePerNumPackets},
    {"PL?", atParamReadPerPacketLen},
    {"pl?", atParamReadPerPacketLen},
    {"PL=", atParamWritePerPacketLen},
    {"pl=", atParamWritePerPacketLen},
    {"TM?", atParamReadTestMode},
    {"tm?", atParamReadTestMode},
    {"TM=", atParamWriteTestMode},
    {"tm=", atParamWriteTestMode},
    {"FR?", atParamReadFreq},
    {"fr?", atParamReadFreq},
    {"FR=", atParamWriteFreq},
    {"fr=", atParamWriteFreq},
    {"PW?", atParamReadPower},
    {"pw?", atParamReadPower},
    {"PW=", atParamWritePower},
    {"pw=", atParamWritePower},
    {"CR?", atParamReadCurrRssi},
    {"cr?", atParamReadCurrRssi},
    {"AR?", atParamReadAvgRssi},
    {"ar?", atParamReadAvgRssi},
    {"LR?", atParamReadMinRssi},
    {"lr?", atParamReadMinRssi},
    {"MR?", atParamReadMaxRssi},
    {"mr?", atParamReadMaxRssi},
    {"PN?", atParamReadPHyNames},
    {"pn?", atParamReadPHyNames},
    {"AE?", atParamReadAtEcho},
    {"ae?", atParamReadAtEcho},
    {"AE=", atParamWriteAtEcho},
    {"ae=", atParamWriteAtEcho},
    {"RV?", atParamReadRadioVersion},
    {"rv?", atParamReadRadioVersion}
};

static AtProcess_Status atParamReadPHyNames(char *paramStr, uint32_t paramLen)
{
    (void)paramLen;

    uint8_t radioId;
    AtTerm_getIdAndParam(paramStr, &radioId, (uintptr_t)NULL, (uintptr_t)NULL, 0);

    for (uint8_t i = 0; i < Radio_getNumSupportedPhys(); i++) {
        AtTerm_sendString(Radio_getPhyName(i));
        AtTerm_sendString("\r\n");
    }
    return AtProcess_Status_Success;
}

static AtProcess_Status atParamReadPerNumPackets(char *paramStr, uint32_t paramLen)
{
    (void)paramStr;
    (void)paramLen;
    AtTerm_sendStringUi32Value("", perNumPkts, 10);
    AtTerm_sendString("\r\n");
    return AtProcess_Status_Success;
}

static AtProcess_Status atParamWritePerNumPackets(char *paramStr, uint32_t paramLen)
{
    (void)paramLen;
    AtProcess_Status status = AtProcess_Status_ParamError;
    uint32_t temp = perNumPkts;
    if (sscanf(paramStr, "%d", (int *)&perNumPkts) == 1) {
        if (perNumPkts <= MAX_NUMBER_OF_PACKETS) {
            status = AtProcess_Status_Success;
        } else {
            perNumPkts = temp;
        }
    }
    return status;
}

static AtProcess_Status atParamReadPerPacketLen(char *paramStr, uint32_t paramLen)
{
    (void)paramStr;
    (void)paramLen;
    AtTerm_sendStringUi32Value("", perPktLen, 10);
    AtTerm_sendString("\r\n");
    return AtProcess_Status_Success;
}

static AtProcess_Status atParamWritePerPacketLen(char *paramStr, uint32_t paramLen)
{
    (void)paramLen;
    AtProcess_Status status = AtProcess_Status_ParamError;
    uint32_t temp = perPktLen;
    if (sscanf(paramStr, "%d", (int *)&perPktLen) == 1) {
        if (Radio_checkPacketLength(&perPktLen)) {
            status = AtProcess_Status_Success;
        } else {
            perPktLen = temp;
        }
    }
    return status;
}

static AtProcess_Status atParamReadTestMode(char *paramStr, uint32_t paramLen)
{
    (void)paramStr;
    (void)paramLen;
    uint8_t testMode = TestMode_read();
    AtTerm_sendStringUi8Value("      ", testMode, 10);
    AtTerm_sendString("\r\n");
    return AtProcess_Status_Success;
}

static AtProcess_Status atParamWriteTestMode(char *paramStr, uint32_t paramLen)
{
    (void)paramLen;
    uint32_t mode = atoi(paramStr);
    TestMode_Status testStatus = TestMode_write(mode, perNumPkts, perPktLen, AtParams_printTestMsg);
    return (testStatus == TestMode_Status_Success) ? AtProcess_Status_Success : AtProcess_Status_Error;
}

static AtProcess_Status atParamReadFreq(char *paramStr, uint32_t paramLen)
{
    (void)paramStr;
    (void)paramLen;
    Radio_getFreq(&frequency);
    if (frequency.freq != (uint32_t)RADIO_ERROR_VALUE) {
        AtTerm_sendStringUi32Value("    ", frequency.freq, 10);
        AtTerm_sendString(" Hz\r\n");
        if (frequency.mdrFreq != 0) {
            AtTerm_sendStringUi32Value("    ", frequency.mdrFreq, 10);
            AtTerm_sendString(" Hz\r\n");
        }
        return AtProcess_Status_Success;
    }
    return AtProcess_Status_Error;
}

static AtProcess_Status atParamWriteFreq(char *paramStr, uint32_t paramLen)
{
    (void)paramLen;
    AtProcess_Status status = AtProcess_Status_ParamError;
    uint32_t newFreq, newMdrFreq;
    uint8_t testMode = TestMode_read();

    if (testMode == TestMode_EXIT) {
        char *token;
        char delimiter[] = " ";
        token = strtok(paramStr, delimiter);
        if (NULL != token) {
            status = AtProcess_Status_Success;
            newFreq = atoi(token);
            token = strtok(NULL, delimiter);
            newMdrFreq = (NULL != token) ? atoi(token) : NO_MDR_FREQ;
            Radio_setFreq(newFreq, newMdrFreq);
        } else {
            status = AtProcess_Status_Error;
        }
    } else {
        status = AtProcess_Status_Error;
    }
    return status;
}

static AtProcess_Status atParamReadPower(char *paramStr, uint32_t paramLen)
{
    (void)paramStr;
    (void)paramLen;
    int8_t power = Radio_getPower();
    if (power != RADIO_ERROR_VALUE) {
        AtTerm_sendStringI8Value("      ", power, 10);
        AtTerm_sendString(" dBm\r\n");
        return AtProcess_Status_Success;
    }
    return AtProcess_Status_Error;
}

static AtProcess_Status atParamWritePower(char *paramStr, uint32_t paramLen)
{
    (void)paramLen;
    AtProcess_Status status = AtProcess_Status_ParamError;
    int32_t newPower;
    uint8_t testMode = TestMode_read();

    if (testMode == TestMode_EXIT) {
        if (sscanf(paramStr, "%d", (int *)&newPower) == 1) {
            status = Radio_setPower(newPower) ? AtProcess_Status_Success : AtProcess_Status_Error;
        }
    } else {
        status = AtProcess_Status_Error;
    }
    return status;
}

static AtProcess_Status atParamReadCurrRssi(char *paramStr, uint32_t paramLen)
{
    (void)paramStr;
    (void)paramLen;
    int8_t rssiDbm = Radio_getCurrentRssi();
    if (rssiDbm != RADIO_ERROR_VALUE) {
        AtTerm_sendStringI8Value("      ", rssiDbm, 10);
        AtTerm_sendString(" dBm\r\n");
        return AtProcess_Status_Success;
    }
    return AtProcess_Status_Error;
}

static AtProcess_Status atParamReadAvgRssi(char *paramStr, uint32_t paramLen)
{
    (void)paramStr;
    (void)paramLen;
    if (TestMode_read() == TestMode_PER_RX) {
        int32_t avgRssi = Radio_getAvgRssi();
        AtTerm_sendStringI32Value("  ", avgRssi, 10);
        AtTerm_sendString(" dBm\r\n");
        return AtProcess_Status_Success;
    }
    return AtProcess_Status_Error;
}

static AtProcess_Status atParamReadMinRssi(char *paramStr, uint32_t paramLen)
{
    (void)paramStr;
    (void)paramLen;
    if (TestMode_read() == TestMode_PER_RX) {
        AtTerm_sendStringI8Value("      ", Radio_getMinRssi(), 10);
        AtTerm_sendString(" dBm\r\n");
        return AtProcess_Status_Success;
    }
    return AtProcess_Status_Error;
}

static AtProcess_Status atParamReadMaxRssi(char *paramStr, uint32_t paramLen)
{
    (void)paramStr;
    (void)paramLen;
    if (TestMode_read() == TestMode_PER_RX) {
        AtTerm_sendStringI8Value("      ", Radio_getMaxRssi(), 10);
        AtTerm_sendString(" dBm\r\n");
        return AtProcess_Status_Success;
    }
    return AtProcess_Status_Error;
}

static AtProcess_Status atParamWriteAtEcho(char *paramStr, uint32_t paramLen)
{
    (void)paramLen;
    AtParams_echoEnabled = atoi(paramStr);
    return AtProcess_Status_Success;
}

static AtProcess_Status atParamReadAtEcho(char *paramStr, uint32_t paramLen)
{
    (void)paramStr;
    (void)paramLen;
    AtTerm_sendStringUi8Value("      ", AtParams_echoEnabled, 10);
    AtTerm_sendString("\r\n");
    return AtProcess_Status_Success;
}

static AtProcess_Status atParamReadRadioVersion(char *paramStr, uint32_t paramLen)
{
    (void)paramStr;
    (void)paramLen;
    char radioVersion[RADIO_VERSION_LENGTH] = {0};
    AtProcess_Status status = AtProcess_Status_ParamError;

    strcpy(radioVersion, Radio_getRadioVersion());
    if (strcmp(radioVersion, RADIO_UNSUPPORTED_CMD)) {
        AtTerm_sendString(radioVersion);
        AtTerm_sendString("\r\n");
        status = AtProcess_Status_Success;
    }
    return status;
}

AtProcess_Status AtParams_parseIncoming(char *param, uint8_t paramLen)
{
    uint8_t cmdIdx, paramOffSet;
    AtProcess_Status status = AtProcess_Status_CmdIdError;

    for (cmdIdx = 0; cmdIdx < (sizeof(atParamsters) / sizeof(AtCommand_t)); cmdIdx++) {
        if ((paramOffSet = AtProcess_cmdCmpAndSetRadioId(atParamsters[cmdIdx].cmdStr, param)) &&
            (atParamsters[cmdIdx].cmdFxn != NULL)) {
            status = atParamsters[cmdIdx].cmdFxn(&(param[paramOffSet]), paramLen - 1);
            break;
        }
    }
    return status;
}

void AtParams_printTestMsg(uint32_t mode, uint16_t packetCount, uint16_t packetCountNok,
                           uint16_t rxSyncCount, bool txDone, int32_t avgRssi,
                           int32_t minRssi, int32_t maxRssi, char *phyName)
{
    if (mode == TestMode_PER_TX || mode == TestMode_MDR_TX ||
        mode == TestMode_MDR_CS_TX || mode == TestMode_CS_TX) {
        if (txDone) {
            AtTerm_sendStringUi16Value("Packets Transmitted: ", packetTxCount, 10);
            AtTerm_sendString("\r\n");
            packetTxCount = 0;
        } else {
            packetTxCount = packetCount;
            if (packetTxCount == 1) {
                AtTerm_sendString("Sending packets....\r\n");
            }
        }
    } else if (mode == TestMode_PER_RX) {
        AtTerm_sendString("RX in Progress....\r\n");
        AtTerm_sendString("Exit by setting Test Mode = 0 (ATPTM=0)\r\n");
    } else if (mode == TestMode_MDR_RX) {
        AtTerm_sendString("MDR RX in Progress....\r\n");
        AtTerm_sendString("Exit by setting Test Mode = 0 (ATPTM=0)\r\n");
    } else {
        uint32_t totalPackets = (uint32_t)(packetCount + packetCountNok);
        uint32_t per = (totalPackets > 0) ? (((uint32_t)packetCountNok * 100) / totalPackets) : 0;

        AtTerm_sendStringUi16Value("Packets Received: ", (packetCount + packetCountNok), 10);
        AtTerm_sendString("\r\n");
        AtTerm_sendStringUi16Value("CRC Ok:           ", packetCount, 10);
        AtTerm_sendString("\r\n");
        AtTerm_sendStringUi16Value("Sync Ok:          ", rxSyncCount, 10);
        AtTerm_sendString("\r\n");
        AtTerm_sendStringUi16Value("PER:              ", (int16_t)per, 10);
        AtTerm_sendString("%\r\n");
        AtTerm_sendStringI32Value("Average RSSI: ", avgRssi, 10);
        AtTerm_sendString(" dBm\r\n");
        AtTerm_sendStringI32Value("Max RSSI:     ", maxRssi, 10);
        AtTerm_sendString(" dBm\r\n");
        AtTerm_sendStringI32Value("Min RSSI:     ", minRssi, 10);
        AtTerm_sendString(" dBm\r\n");
        AtTerm_sendString("PHY RX'd: ");
        AtTerm_sendString(phyName);
        AtTerm_sendString("\r\n");
    }
}
