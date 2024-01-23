#include "ll.h"
#include "ll_csdrbg.h"
#include "map_direct.h"

#define UINT8_SIZE_128 16
#define UINT8_SIZE_64 8

static uint8 randCheckArry[][UINT8_SIZE_128] = {
    {0x79, 0x74, 0x1F, 0xD1, 0x8F, 0x57, 0x7B, 0x45, 0xD0, 0x9A, 0x66, 0x5A,
     0x7F, 0x1F, 0x28, 0x58},
    {0xD1, 0xC1, 0xD0, 0x5A, 0x40, 0xB4, 0xC4, 0x81, 0xEF, 0xBB, 0x39, 0xB2,
     0x61, 0xD2, 0x9C, 0x4E},
    {0x71, 0xFD, 0xE8, 0x68, 0xE8, 0xCA, 0xCA, 0xD1, 0x18, 0xE5, 0x9B, 0x18,
     0x5C, 0xEE, 0xFC, 0x17},
    {0xFF, 0xF8, 0x74, 0x38, 0x4E, 0x1C, 0xB9, 0xD3, 0xE6, 0x9F, 0x1F, 0x2C,
     0x2A, 0x22, 0xCF, 0x63},
    {0x5F, 0x1B, 0x11, 0x5A, 0x00, 0x40, 0xB4, 0x2C, 0x2F, 0x8F, 0xB1, 0x0C,
     0x9B, 0x6F, 0xD7, 0xAC},
    {0x55, 0x73, 0xA6, 0xA5, 0x20, 0x91, 0xB1, 0x58, 0x99, 0xC8, 0x64, 0xAB,
     0x2F, 0xB0, 0xE2, 0x02},
    {0x46, 0x20, 0xD2, 0x01, 0xB7, 0x41, 0x9D, 0x82, 0x48, 0x68, 0xD5, 0xA7,
     0xC8, 0x20, 0xBA, 0x2C},
    {0xB4, 0x20, 0x00, 0x40, 0x63, 0xB1, 0x13, 0xA2, 0x3E, 0xD0, 0x43, 0xD0,
     0x3B, 0x6E, 0xE7, 0x34},
    {0x6A, 0xEA, 0xB7, 0x5D, 0x82, 0x52, 0x58, 0x31, 0x42, 0x2C, 0xE2, 0xF4,
     0x2E, 0x85, 0x07, 0x88},
    {0x14, 0x40, 0xEF, 0x44, 0x74, 0xAF, 0xD8, 0x49, 0x32, 0xFE, 0x37, 0xB0,
     0xA4, 0x10, 0xF2, 0x3A},
    {0x2A, 0x83, 0xEF, 0x11, 0xCC, 0xE5, 0x99, 0x31, 0x4E, 0xFB, 0x29, 0x03,
     0xF4, 0xDF, 0xD4, 0xB8},
    {0x19, 0x6B, 0xE2, 0xB6, 0x43, 0x98, 0x53, 0xFE, 0xD4, 0x96, 0x67, 0x54,
     0xE1, 0x40, 0x15, 0x1E},
    {0x58, 0x04, 0xC1, 0x73, 0x69, 0xC5, 0x64, 0xDC, 0x73, 0xD6, 0xE7, 0x4E,
     0xA0, 0xC6, 0xE1, 0x66}};
static uint8 K_CheckArry[][UINT8_SIZE_128] = {
    {0xEE, 0xE0, 0x4D, 0x7C, 0x76, 0x11, 0x3A, 0x5C, 0xEC, 0x99, 0x2A, 0xE3,
     0x20, 0xC2, 0x4D, 0x27},
    {0xF3, 0xF4, 0xCF, 0x24, 0x5F, 0xCE, 0x86, 0xB3, 0xC6, 0x9F, 0xAE, 0x60,
     0x28, 0x7E, 0xB5, 0xAE},
    {0xA9, 0xAA, 0x68, 0xC2, 0x1D, 0xF4, 0x09, 0x00, 0x1A, 0xF9, 0x27, 0xAC,
     0x14, 0xC8, 0x18, 0x96},
    {0x77, 0x71, 0x3B, 0xB1, 0x81, 0xC3, 0xB6, 0x44, 0x29, 0xE8, 0x98, 0xB8,
     0x65, 0x9B, 0x85, 0xA9}};
static uint8 V_CheckArry[][UINT8_SIZE_128] = {
    {0xDF, 0x90, 0x56, 0x47, 0xC1, 0x06, 0x6E, 0x6F, 0x52, 0xC0, 0x3E, 0xDF,
     0xB8, 0x2B, 0x69, 0x28},
    {0x08, 0x56, 0x98, 0x94, 0x89, 0x1F, 0x2F, 0x30, 0x1E, 0xEC, 0xDB, 0x38,
     0xB6, 0xED, 0x54, 0xB1},
    {0x16, 0x07, 0xDE, 0x6C, 0xFC, 0x80, 0xD2, 0x18, 0x64, 0xF7, 0x08, 0xA9,
     0x9C, 0x49, 0x39, 0x96},
    {0xD5, 0x88, 0x73, 0x99, 0x44, 0x68, 0x36, 0x96, 0xA9, 0x3D, 0x36, 0x19,
     0x3C, 0xDA, 0xBA, 0x11}};

uint8 current_rand = 0;
uint8 current_k_v = 0;

uint8 csDrbgSelfTest()
{
    uint8 CS_IV[UINT8_SIZE_128] = {0xE1, 0x0B, 0xC2, 0x8A, 0x0B, 0xFD,
                                   0xDF, 0xE9, 0x3E, 0x7F, 0x51, 0x86,
                                   0xE0, 0xCA, 0x0B, 0x3B};
    uint8 CS_IN[UINT8_SIZE_64] = {0x9F, 0xF4, 0x77, 0xC1,
                                  0x86, 0x73, 0x84, 0x0D};
    uint8 CS_PV[UINT8_SIZE_128] = {0xC9, 0x80, 0xDE, 0xDF, 0x98, 0x82,
                                   0xED, 0x44, 0x64, 0xA6, 0x74, 0x96,
                                   0x78, 0x68, 0xF1, 0x43};

    uint8 randomBits[UINT8_SIZE_128] = {0};
    uint16 CSStepCounter = 0x0000;
    uint8 TransactionID = 0x00;
    uint8 TransactionCounter = 0x00;

    drbgParams_t params;
    params.csProcedureCounter = 0;
    params.CSStepCounter = CSStepCounter;
    params.TransactionID = TransactionID;
    params.TransactionCounter = TransactionCounter;

    // first proc.
    LL_CSDRBG_Init(CS_IV, CS_IN, CS_PV, &params);
    if ((MAP_osal_memcmp(params.kDrbg, K_CheckArry[current_k_v],
                         UINT8_SIZE_128) == FALSE) ||
        (MAP_osal_memcmp(params.vDrbg, V_CheckArry[current_k_v],
                         UINT8_SIZE_128) == FALSE))
    {
        return FALSE;
    }
    current_k_v += 1;

    // Proc. cnt. = 0; Step cnt. = 0; Transaction ID = 0; Transaction cnt. =  0
    LL_CSDRBG_GetDrbg(randomBits, &params);
    if ((MAP_osal_memcmp(randomBits, randCheckArry[current_rand],
                         UINT8_SIZE_128)) == FALSE)
    {
        return FALSE;
    }
    current_rand += 1;

    // Proc. cnt. = 0; Step cnt. = 0; Transaction ID = 0; Transaction cnt. =  1
    params.TransactionCounter = 0x01;
    LL_CSDRBG_GetDrbg(randomBits, &params);
    if ((MAP_osal_memcmp(randomBits, randCheckArry[current_rand],
                         UINT8_SIZE_128)) == FALSE)
    {
        return FALSE;
    }
    current_rand += 1;

    // Proc. cnt. = 0; Step cnt. = 0; Transaction ID = 0; Transaction cnt. =  2
    params.TransactionCounter = 0x02;
    LL_CSDRBG_GetDrbg(randomBits, &params);
    if ((MAP_osal_memcmp(randomBits, randCheckArry[current_rand],
                         UINT8_SIZE_128)) == FALSE)
    {
        return FALSE;
    }
    current_rand += 1;

    // Proc. cnt. = 0; Step cnt. = 10; Transaction ID = 4; Transaction cnt. =  0
    params.CSStepCounter = 0x000A;
    params.TransactionID = 0x04;
    params.TransactionCounter = 0x00;
    LL_CSDRBG_GetDrbg(randomBits, &params);
    if ((MAP_osal_memcmp(randomBits, randCheckArry[current_rand],
                         UINT8_SIZE_128)) == FALSE)
    {
        return FALSE;
    }
    current_rand += 1;

    // f9() update - Last CS step in the previous procedure: 10
    params.TransactionID = 0x09;
    LL_CSDRBG_BackTracking(&params);

    if ((MAP_osal_memcmp(params.kDrbg, K_CheckArry[current_k_v],
                         UINT8_SIZE_128) == FALSE) ||
        (MAP_osal_memcmp(params.vDrbg, V_CheckArry[current_k_v],
                         UINT8_SIZE_128) == FALSE))
    {
        return FALSE;
    }
    current_k_v += 1;

    // Proc. cnt. = 1; Step cnt. = 0; Transaction ID = 0; Transaction cnt. =  0
    params.CSStepCounter = 0x0000;
    params.TransactionID = 0x00;
    params.TransactionCounter = 0x00;
    LL_CSDRBG_GetDrbg(randomBits, &params);
    if ((MAP_osal_memcmp(randomBits, randCheckArry[current_rand],
                         UINT8_SIZE_128)) == FALSE)
    {
        return FALSE;
    }
    current_rand += 1;

    // Proc. cnt. = 1; Step cnt. = 0; Transaction ID = 0; Transaction cnt. =  1
    params.TransactionCounter = 0x01;
    LL_CSDRBG_GetDrbg(randomBits, &params);
    if ((MAP_osal_memcmp(randomBits, randCheckArry[current_rand],
                         UINT8_SIZE_128)) == FALSE)
    {
        return FALSE;
    }
    current_rand += 1;

    // Proc. cnt. = 1; Step cnt. = 0; Transaction ID = 0; Transaction cnt. =  2
    params.TransactionCounter = 0x02;
    LL_CSDRBG_GetDrbg(randomBits, &params);
    if ((MAP_osal_memcmp(randomBits, randCheckArry[current_rand],
                         UINT8_SIZE_128)) == FALSE)
    {
        return FALSE;
    }
    current_rand += 1;

    // Proc. cnt. = 1; Step cnt. = 14; Transaction ID = 4; Transaction cnt. =  0
    params.CSStepCounter = 0x000E;
    params.TransactionID = 0x04;
    params.TransactionCounter = 0x00;
    LL_CSDRBG_GetDrbg(randomBits, &params);
    if ((MAP_osal_memcmp(randomBits, randCheckArry[current_rand],
                         UINT8_SIZE_128)) == FALSE)
    {
        return FALSE;
    }
    current_rand += 1;

    // f9() update: Last CS step in the previous procedure: 14
    params.TransactionID = 0x09;
    LL_CSDRBG_BackTracking(&params);
    if ((MAP_osal_memcmp(params.kDrbg, K_CheckArry[current_k_v],
                         UINT8_SIZE_128) == FALSE) ||
        (MAP_osal_memcmp(params.vDrbg, V_CheckArry[current_k_v],
                         UINT8_SIZE_128) == FALSE))
    {
        return FALSE;
    }
    current_k_v += 1;

    // Proc. cnt. = 2; Step cnt. = 0; Transaction ID = 0; Transaction cnt. =  0
    params.CSStepCounter = 0x0000;
    params.TransactionID = 0x00;
    params.TransactionCounter = 0x00;
    LL_CSDRBG_GetDrbg(randomBits, &params);
    if ((MAP_osal_memcmp(randomBits, randCheckArry[current_rand],
                         UINT8_SIZE_128)) == FALSE)
    {
        return FALSE;
    }
    current_rand += 1;

    // Proc. cnt. = 2; Step cnt. = 1; Transaction ID = 0; Transaction cnt. =  0
    params.CSStepCounter = 0x0001;
    LL_CSDRBG_GetDrbg(randomBits, &params);
    if ((MAP_osal_memcmp(randomBits, randCheckArry[current_rand],
                         UINT8_SIZE_128)) == FALSE)
    {
        return FALSE;
    }
    current_rand += 1;

    // Proc. cnt. = 2; Step cnt. = 2; Transaction ID = 0; Transaction cnt. =  0
    params.CSStepCounter = 0x0002;
    LL_CSDRBG_GetDrbg(randomBits, &params);
    if ((MAP_osal_memcmp(randomBits, randCheckArry[current_rand],
                         UINT8_SIZE_128)) == FALSE)
    {
        return FALSE;
    }
    current_rand += 1;

    // Proc. cnt. = 2; Step cnt. = 3; Transaction ID = 0; Transaction cnt. =  0
    params.CSStepCounter = 0x0003;
    LL_CSDRBG_GetDrbg(randomBits, &params);
    if ((MAP_osal_memcmp(randomBits, randCheckArry[current_rand],
                         UINT8_SIZE_128)) == FALSE)
    {
        return FALSE;
    }
    current_rand += 1;

    // f9() update: Last CS step in the previous procedure: 3
    params.TransactionID = 0x09;

    LL_CSDRBG_BackTracking(&params);
    if ((MAP_osal_memcmp(params.kDrbg, K_CheckArry[current_k_v],
                         UINT8_SIZE_128) == FALSE) ||
        (MAP_osal_memcmp(params.vDrbg, V_CheckArry[current_k_v],
                         UINT8_SIZE_128) == FALSE))
    {
        return FALSE;
    }
    current_k_v += 1;

    // Proc. cnt. = 3; Step cnt. = 0; Transaction ID = 0; Transaction cnt. =  0
    params.CSStepCounter = 0x0000;
    params.TransactionID = 0x00;
    params.TransactionCounter = 0x00;
    LL_CSDRBG_GetDrbg(randomBits, &params);
    if ((MAP_osal_memcmp(randomBits, randCheckArry[current_rand],
                         UINT8_SIZE_128)) == FALSE)
    {
        return FALSE;
    }
    current_rand += 1;
    return TRUE;
}
