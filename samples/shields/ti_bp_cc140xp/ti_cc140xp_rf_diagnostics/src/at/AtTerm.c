/*
 * Copyright (c) 2026 Texas Instruments Incorporated
 * SPDX-License-Identifier: Apache-2.0
 *
 * AT command terminal I/O, implemented with Zephyr UART polling.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include "at/AtTerm.h"
#include "at/AtProcess.h"

static const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

int32_t AtTerm_init(void)
{
    if (!device_is_ready(uart_dev)) {
        while (1) {}
    }
    return 0;
}

int32_t AtTerm_getChar(char *ch)
{
    unsigned char c;
    while (uart_poll_in(uart_dev, &c) != 0) {
        /* spin until character is available */
    }
    *ch = (char)c;
    return 1;
}

void AtTerm_putChar(char ch)
{
    uart_poll_out(uart_dev, (unsigned char)ch);
}

void AtTerm_sendString(char *string)
{
    while (*string) {
        uart_poll_out(uart_dev, (unsigned char)*string++);
    }
}

void AtTerm_sendStringUi8Value(char *string, uint8_t value, uint8_t format)
{
    char buf[16];
    AtTerm_sendString(string);
    if (format == 16) {
        snprintf(buf, sizeof(buf), "%X", value);
    } else {
        snprintf(buf, sizeof(buf), "%u", value);
    }
    AtTerm_sendString(buf);
}

void AtTerm_sendStringI8Value(char *string, int8_t value, uint8_t format)
{
    char buf[16];
    AtTerm_sendString(string);
    if (format == 16) {
        snprintf(buf, sizeof(buf), "%X", (uint8_t)value);
    } else {
        snprintf(buf, sizeof(buf), "%d", value);
    }
    AtTerm_sendString(buf);
}

void AtTerm_sendStringUi16Value(char *string, uint16_t value, uint8_t format)
{
    char buf[16];
    AtTerm_sendString(string);
    if (format == 16) {
        snprintf(buf, sizeof(buf), "%X", value);
    } else {
        snprintf(buf, sizeof(buf), "%u", value);
    }
    AtTerm_sendString(buf);
}

void AtTerm_sendStringI16Value(char *string, int16_t value, uint8_t format)
{
    char buf[16];
    AtTerm_sendString(string);
    if (format == 16) {
        snprintf(buf, sizeof(buf), "%X", (uint16_t)value);
    } else {
        snprintf(buf, sizeof(buf), "%d", value);
    }
    AtTerm_sendString(buf);
}

void AtTerm_sendStringUi32Value(char *string, uint32_t value, uint8_t format)
{
    char buf[16];
    AtTerm_sendString(string);
    if (format == 16) {
        snprintf(buf, sizeof(buf), "%lX", (unsigned long)value);
    } else {
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)value);
    }
    AtTerm_sendString(buf);
}

void AtTerm_sendStringI32Value(char *string, int32_t value, uint8_t format)
{
    char buf[16];
    AtTerm_sendString(string);
    if (format == 16) {
        snprintf(buf, sizeof(buf), "%lX", (unsigned long)(uint32_t)value);
    } else {
        snprintf(buf, sizeof(buf), "%ld", (long)value);
    }
    AtTerm_sendString(buf);
}

void AtTerm_clearTerm(void)
{
    uart_poll_out(uart_dev, '\f');
}

void AtTerm_getIdAndParam(char *paramStr, uint8_t *radioId, uintptr_t fxnParam,
                          uintptr_t fxnParam2, size_t fxnParamLen)
{
    char *token;
    char delimiter[] = " ";

    token = strtok(paramStr, delimiter);
    if (NULL != token) {
        if (NULL != radioId) {
            *radioId = (uint8_t)atoi(token);
        }
        token = strtok(NULL, delimiter);
        if ((NULL != token) && (0 != fxnParamLen)) {
            uint8_t val = (uint8_t)atoi(token);
            memcpy((void *)fxnParam, &val, fxnParamLen);
            token = strtok(NULL, delimiter);
            if (NULL != token) {
                uint8_t val2 = (uint8_t)atoi(token);
                memcpy((void *)fxnParam2, &val2, fxnParamLen);
            }
        }
    }
}
