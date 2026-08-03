/*
 * Copyright (c) 2026 Texas Instruments Incorporated
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr adaptation: replaces SysCtrlSystemReset with sys_reboot,
 * removes DeviceFamily_constructPath includes.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/sys/reboot.h>

#include "at/AtProcess.h"
#include "at/AtControl.h"
#include "at/AtTerm.h"
#include "at/DbgPrint.h"
#include "at/AtParams.h"
#include "radio/radio.h"
#include <TestMode.h>

static AtProcess_Status atCtrlrFInit(char *paramStr, uint32_t paramLen);
static AtProcess_Status atCtrlReset(char *paramStr, uint32_t paramLen);
static AtProcess_Status atCtrlEnableMdr(char *paramStr, uint32_t paramLen);

static AtCommand_t atControlCmds[] =
    {
        {"I", atCtrlrFInit},
        {"i", atCtrlrFInit},
        {"RS", atCtrlReset},
        {"rs", atCtrlReset},
        {"EM", atCtrlEnableMdr},
        {"em", atCtrlEnableMdr}
};

static AtProcess_Status atCtrlReset(char *paramStr, uint32_t paramLen)
{
    (void)paramStr;
    (void)paramLen;

    sys_reboot(SYS_REBOOT_COLD);
    return AtProcess_Status_Success;
}

static AtProcess_Status atCtrlEnableMdr(char *paramStr, uint32_t paramLen)
{
    (void)paramLen;
    uint8_t testMode = TestMode_read();
    uint8_t radioId = 0;

    AtProcess_Status status = AtProcess_Status_Error;

    if (testMode == TestMode_EXIT) {
        char *token;
        char delimiter[] = " ";
        token = strtok(paramStr, delimiter);
        uint8_t region = 0;
        if (NULL != token) {
            radioId = atoi(token);
            token = strtok(NULL, delimiter);
            if (NULL != token) {
                region = atoi(token);
            }
        }
        if (Radio_verifyRadioId(radioId)) {
            Radio_setCurrentRadio(radioId);
            if (Radio_enableMdr(region)) {
                status = AtProcess_Status_Success;
            }
        }
    }

    return status;
}

static AtProcess_Status atCtrlrFInit(char *paramStr, uint32_t paramLen)
{
    (void)paramLen;
    uint8_t phyIndex = 0;
    uint8_t phyIndex2 = RADIO_NO_PHY;
    uint8_t radioId;
    uint8_t testMode = TestMode_read();

    AtProcess_Status status = AtProcess_Status_Error;

    if (testMode == TestMode_EXIT) {
        AtTerm_getIdAndParam(paramStr, &radioId, (uintptr_t)&phyIndex, (uintptr_t)&phyIndex2, sizeof(phyIndex));
        if (Radio_verifyRadioId(radioId)) {
            Radio_setCurrentRadio(radioId);

            if (phyIndex < Radio_getNumSupportedPhys() &&
                (phyIndex2 < Radio_getNumSupportedPhys() || phyIndex2 == RADIO_NO_PHY)) {
                bool radioStatus = Radio_setupPhy(phyIndex, phyIndex2);

                if (!radioStatus) {
                    status = AtProcess_Status_Error;
                    AtTerm_sendString("Init Failed \r\n");
                } else {
                    status = AtProcess_Status_Success;
                    AtTerm_sendString("Init State \r\n");
                }
            }
        }
    }

    return status;
}

AtProcess_Status AtControl_parseIncoming(char *command, uint8_t cmdLen)
{
    uint8_t cmdIdx, paramOffSet;
    AtProcess_Status status = AtProcess_Status_CmdIdError;

    for (cmdIdx = 0; cmdIdx < (sizeof(atControlCmds) / sizeof(AtCommand_t)); cmdIdx++) {
        if ((paramOffSet = AtProcess_cmdCmpAndSetRadioId(atControlCmds[cmdIdx].cmdStr, command)) &&
            (atControlCmds[cmdIdx].cmdFxn != NULL)) {
            status = atControlCmds[cmdIdx].cmdFxn(&(command[paramOffSet]), cmdLen - paramOffSet);
            break;
        }
    }
    return status;
}
