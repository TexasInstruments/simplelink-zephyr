// SETTINGS FOR PHY BASED ON RADIO CONTROL LAYER (SOURCE FILE)
//
//
// CODE EXPORT INFORMATION
// This file is generated
//
// Tool name                SmartRF Studio 8
// Tool version             1.4.0.97
//
//
// WORKSPACE INFORMATION
//
// Workspace file           tempws
// Device                   CC1407P
//     Package              QFN40 5x5 RKP
//     Revision(s)          A (1.0)
// SDK                      SimpleLink CC1407 SDK 9.22.00.01
// Board                    BP-EM-CC1407P-SE
// PHY                      779 - 930 MHz - IEEE 802.15.4 - SUN O-QPSK
//     State                Experimental
// PHY abbreviation         sunoqpsk
//
//
// PHY PROPERTIES
//
// Run-time properties:
//     Frequency            915.200 MHz
//     TX output power      14.0 dBm
//     Rate mode in TX      1 (12.5 kb/s)

#include "rcl_settings_sunoqpsk.h"


// Configuration: FRONTEND
const uint32_t LRF_frontendRegConfig_sunoqpsk[] =
{
    0x00008043,    // 1 segment(s), total length = 67
    0x00000042,    // Segment length = 66
    0x00249018,    //     Data structure 32-bit no-copy region (pointer byte position = 36, count = 25)
    0x00000018,    //         TRX_paTable : numEntries
    0x001603C4,    //         TRX_paTable : powerTable
    0x0A1503D8,    //         TRX_paTable : -
    0x0A5703E2,    //         TRX_paTable : -
    0x0A9B0BEC,    //         TRX_paTable : -
    0x142C07F6,    //         TRX_paTable : -
    0x143207F8,    //         TRX_paTable : -
    0x149613FA,    //         TRX_paTable : -
    0x16230FFC,    //         TRX_paTable : -
    0x16270FFE,    //         TRX_paTable : -
    0x162C0F00,    //         TRX_paTable : -
    0x16320F02,    //         TRX_paTable : -
    0x16601704,    //         TRX_paTable : -
    0x16651706,    //         TRX_paTable : -
    0x166A1708,    //         TRX_paTable : -
    0x169B1F0A,    //         TRX_paTable : -
    0x169F1F0C,    //         TRX_paTable : -
    0x16A5230E,    //         TRX_paTable : -
    0x16AA2710,    //         TRX_paTable : -
    0x16DC2712,    //         TRX_paTable : -
    0x16E32F14,    //         TRX_paTable : -
    0x17163316,    //         TRX_paTable : -
    0x17503718,    //         TRX_paTable : -
    0x17D1421A,    //         TRX_paTable : -
    0x1FFF001C,    //         TRX_paTable : -
    0x00289026,    //     Data structure 32-bit no-copy region (pointer byte position = 40, count = 39)
    0x00000013,    //         TRX_paTableHigh : numEntries
    0x00000000,    //         TRX_paTableHigh : powerTable
    0x10058076,    //         TRX_paTableHigh : -
    0x0000000A,    //         TRX_paTableHigh : -
    0x16558080,    //         TRX_paTableHigh : -
    0x00000014,    //         TRX_paTableHigh : -
    0x15588080,    //         TRX_paTableHigh : -
    0x00000016,    //         TRX_paTableHigh : -
    0x0E588080,    //         TRX_paTableHigh : -
    0x00000018,    //         TRX_paTableHigh : -
    0x0E5A8076,    //         TRX_paTableHigh : -
    0x0000001A,    //         TRX_paTableHigh : -
    0x0D798070,    //         TRX_paTableHigh : -
    0x0000001C,    //         TRX_paTableHigh : -
    0x0D7A8070,    //         TRX_paTableHigh : -
    0x0000001E,    //         TRX_paTableHigh : -
    0x0F798070,    //         TRX_paTableHigh : -
    0x00000020,    //         TRX_paTableHigh : -
    0x0F7A8076,    //         TRX_paTableHigh : -
    0x00000022,    //         TRX_paTableHigh : -
    0x0FF98070,    //         TRX_paTableHigh : -
    0x00000023,    //         TRX_paTableHigh : -
    0x0FF98076,    //         TRX_paTableHigh : -
    0x00000024,    //         TRX_paTableHigh : -
    0x0FF98080,    //         TRX_paTableHigh : -
    0x00000025,    //         TRX_paTableHigh : -
    0x0FFA807D,    //         TRX_paTableHigh : -
    0x00000026,    //         TRX_paTableHigh : -
    0x07F8807D,    //         TRX_paTableHigh : -
    0x00000027,    //         TRX_paTableHigh : -
    0x07F9807D,    //         TRX_paTableHigh : -
    0x00000028,    //         TRX_paTableHigh : -
    0x07FA8080,    //         TRX_paTableHigh : -
    0x00000029,    //         TRX_paTableHigh : -
    0x07FC8080,    //         TRX_paTableHigh : -
    0x0000002A,    //         TRX_paTableHigh : -
    0x07FE8080,    //         TRX_paTableHigh : -
    0x0000002B,    //         TRX_paTableHigh : -
    0x07FF8080     //         TRX_paTableHigh : -
};

// Configuration length: FRONTEND
const uint32_t LRF_frontendRegConfig_sunoqpsk_byteCount = sizeof(LRF_frontendRegConfig_sunoqpsk);


// Configuration: MAIN
const uint32_t LRF_mainRegConfig_sunoqpsk[] =
{
    0x00008072,    // 1 segment(s), total length = 114
    0x00000071,    // Segment length = 113
    0x00389005,    //     Data structure 32-bit no-copy region (pointer byte position = 56, count = 6)
    0x00240002,    //         LRF_shapeBaseOqpsk : scale, numCoeff
    0x00970018,    //         LRF_shapeBaseOqpsk : coeff
    0x00B30020,    //         LRF_shapeBaseOqpsk : -
    0x014B0018,    //         LRF_shapeBaseOqpsk : -
    0x00000000,    //         LRF_shapeBaseOqpsk : -
    0x00000000,    //         LRF_shapeBaseOqpsk : -
    0x0000A001,    //     Data structure 32-bit region (start byte position = 0, count = 2)
    0x00055730,    //         TRX_swParam : rxIntFrequency0
    0xFFFAA8D0,    //         TRX_swParam : rxIntFrequency1
    0x00088002,    //     Data structure zero region (start byte position = 8, count = 3)
    0x0014A002,    //     Data structure 32-bit region (start byte position = 20, count = 3)
    0x0000005C,    //         TRX_swParam : radioRequirements.defaultModem, radioRequirements.supportedRfModes
    0x00000700,    //         TRX_swParam : dcDcSelection.dcDcTx.dtime, dcDcSelection.dcDcTx.ipeak, dcDcSelection.dcDcRx.dtime, dcDcSelection.dcDcRx.ipeak
    0x00000000,    //         TRX_swParam : sniffmodeTimeout
    0x115C1001,    //     HW 16-bit region (start address = 0x115C, count = 2)
    0x00011219,    //         LRFDPBE.SPARE1           PBE_FW.PKTCFG
    0x00003011,    //     HW sparse region (address/value pairs, count = 18)
    0x30640000,    //         LRFDRFE.TXENABLE
    0x30A40E00,    //         LRFDRFE.RX0CODC0
    0x30B80141,    //         LRFDRFE.RX0BDEC
    0x30C00015,    //         LRFDRFE.RX0CICHF
    0x30E40040,    //         LRFDRFE.RX1BDEC
    0x30EC0015,    //         LRFDRFE.RX1CICHF
    0x311082A0,    //         RFE_FW.AGCCFG0
    0x317C0092,    //         LRFDRFE.IQIC
    0x31EC0000,    //         LRFDRFE.IFADC2
    0x32600000,    //         LRFDRFE.TXOFDMLDO
    0x3288D820,    //         LRFDRFE.MISC0
    0x32980C07,    //         LRFDRFE.PHEDISC
    0x33C80055,    //         RFE_FW.AGCCFG8
    0x804C0040,    //         OQE_MAIN_FW.FB2PLLSCALE
    0x80D8000D,    //         LRFDOQE.DEMMISC0
    0x80E4001F,    //         LRFDOQE.MODSYMMAP0
    0x81500005,    //         LRFDOQE.DEMFIFE0
    0x81A82F7F,    //         LRFDOQE.DEMC1BE12
    0x30C81002,    //     HW 16-bit region (start address = 0x30C8, count = 3)
    0x18FF140F,    //         LRFDRFE.RX0MAGNCTRL1     LRFDRFE.RX0MAGNCTRL0
    0x0000140F,    //         -                        LRFDRFE.RX0MAGNCTRL2
    0x30F41002,    //     HW 16-bit region (start address = 0x30F4, count = 3)
    0x18FF140F,    //         LRFDRFE.RX1MAGNCTRL1     LRFDRFE.RX1MAGNCTRL0
    0x0000140F,    //         -                        LRFDRFE.RX1MAGNCTRL2
    0x31181001,    //     HW 16-bit region (start address = 0x3118, count = 2)
    0x206A9804,    //         RFE_FW.AGCCFG3           RFE_FW.AGCCFG2
    0x319C1001,    //     HW 16-bit region (start address = 0x319C, count = 2)
    0x00010212,    //         LRFDRFE.RFECTL1          LRFDRFE.RFECTL0
    0x31BC1001,    //     HW 16-bit region (start address = 0x31BC, count = 2)
    0x0076470A,    //         LRFDRFE.IFAMP            LRFDRFE.LNA
    0x31C81007,    //     HW 16-bit region (start address = 0x31C8, count = 8)
    0x02060084,    //         LRFDRFE.ULNA             LRFDRFE.PA
    0x00000000,    //         LRFDRFE.IFADC0           LRFDRFE.RFLDO
    0x00000000,    //         LRFDRFE.IFADCLF          LRFDRFE.IFADC1
    0x00004000,    //         LRFDRFE.IFADCALDO        LRFDRFE.IFADCQUANT
    0x31F40002,    //     HW zero region (start address = 0x31F4, count = 3)
    0x32001016,    //     HW 16-bit region (start address = 0x3200, count = 23)
    0x01040800,    //         LRFDRFE.DIV              LRFDRFE.DIVCTL
    0x00000000,    //         LRFDRFE.TDCLDO           LRFDRFE.DIVLDO
    0x00000002,    //         LRFDRFE.DCOLDO1          LRFDRFE.DCOLDO0
    0x00000003,    //         LRFDRFE.DIVPDET          LRFDRFE.DCO
    0x55504000,    //         LRFDRFE.TXOFDM1          LRFDRFE.TXOFDM0
    0x00100005,    //         LRFDRFE.TXOFDM3          LRFDRFE.TXOFDM2
    0x00000368,    //         LRFDRFE.TXOFDM5          LRFDRFE.TXOFDM4
    0x805448D8,    //         LRFDRFE.TXOFDM7          LRFDRFE.TXOFDM6
    0x00100002,    //         LRFDRFE.TXOFDM9          LRFDRFE.TXOFDM8
    0x0022A666,    //         LRFDRFE.TXOFDM11         LRFDRFE.TXOFDM10
    0x50420000,    //         LRFDRFE.TXOFDM13         LRFDRFE.TXOFDM12
    0x00000028,    //         -                        LRFDRFE.TXOFDM14
    0x32681005,    //     HW 16-bit region (start address = 0x3268, count = 6)
    0x00000807,    //         LRFDRFE.PRE1             LRFDRFE.PRE0
    0x06030605,    //         LRFDRFE.PRE3             LRFDRFE.PRE2
    0x7F004008,    //         LRFDRFE.CAL1             LRFDRFE.CAL0
    0x32A01001,    //     HW 16-bit region (start address = 0x32A0, count = 2)
    0x0410DF7F,    //         LRFDRFE.PLLMON1          LRFDRFE.PLLMON0
    0x3370100F,    //     HW 16-bit region (start address = 0x3370, count = 16)
    0x76543210,    //         LRFDRFE.DCOHFT1          LRFDRFE.DCOHFT0
    0xFEDCBA98,    //         LRFDRFE.DCOHFT3          LRFDRFE.DCOHFT2
    0x76543210,    //         LRFDRFE.DCOHFB1          LRFDRFE.DCOHFB0
    0xFEDCBA98,    //         LRFDRFE.DCOHFB3          LRFDRFE.DCOHFB2
    0x00475090,    //         RFE_FW.PHYRX0RSSI        LRFDRFE.SPARE0
    0x12210047,    //         RFE_FW.PHYAGCCFG4        RFE_FW.PHYRX1RSSI
    0x20000090,    //         RFE_FW.RXFRONTEND        RFE_FW.RXCODCCFG2
    0x00000000,    //         RFE_FW.IFADCPEAKTHR      RFE_FW.AGCCFG5
    0x33B80002,    //     HW zero region (start address = 0x33B8, count = 3)
    0x80541002,    //     HW 16-bit region (start address = 0x8054, count = 3)
    0x00020001,    //         LRFDOQE.SPARE5           LRFDOQE.SPARE4
    0x00000002,    //         -                        LRFDOQE.SPARE6
    0x806C1001,    //     HW 16-bit region (start address = 0x806C, count = 2)
    0x00090020,    //         LRFDOQE.SPARE11          OQE_MAIN_FW.PREAMBLE
    0x80F01002,    //     HW 16-bit region (start address = 0x80F0, count = 3)
    0xA00F0800,    //         LRFDOQE.BAUDPRE          LRFDOQE.BAUD
    0x00000001,    //         -                        LRFDOQE.BAUDCOMP
    0x00186000,    //     RAM 32-bit region (start address = 0x0018, count = 1)
    0x0518A148,    //         PBE_RAM.CSCTL3DEFAULT    PBE_RAM.CSCTL2DEFAULT
    0x68046004,    //     RAM 32-bit region (start address = 0x6804, count = 5)
    0x00000300,    //         RFE_RAM.TDCCAL1          RFE_RAM.TDCCAL0
    0x04000010,    //         RFE_RAM.TDCPLL           RFE_RAM.TDCCAL2
    0x010A569B,    //         RFE_RAM.K1MSB            RFE_RAM.K1LSB
    0x856B0383,    //         RFE_RAM.K2AL             RFE_RAM.K2BL
    0x02E02659,    //         RFE_RAM.K3AL             RFE_RAM.K3BL
    0x681E5018,    //     RAM 16-bit region (start address = 0x681E, count = 25)
    0x00000000,    //         RFE_RAM.RTRIMMIN         RFE_RAM.RTRIMOFF
    0x49040104,    //         RFE_RAM.DIVF             RFE_RAM.DIVI
    0x00000000,    //         RFE_RAM.DIVLDOF          RFE_RAM.DIVLDOI
    0x01380014,    //         RFE_RAM.LDOSETTLE        RFE_RAM.DIVLDOIOFF
    0x00180000,    //         RFE_RAM.DIVSETTLE        RFE_RAM.CHRGSETTLE
    0x00000000,    //         RFE_RAM.RFLDORX          RFE_RAM.RFLDOTX
    0x00000000,    //         RFE_RAM.RTRIMTX          RFE_RAM.RFLDOOFDMTX
    0x00480000,    //         RFE_RAM.DCOAMPSETTLE     RFE_RAM.RTRIMRX
    0x00FF0000,    //         RFE_RAM.DCOLDO0VMAX      RFE_RAM.DCOLDO0VTRIM
    0x0FFF3FF3,    //         RFE_RAM.CALTIMEOUT       RFE_RAM.DCOOVR0INIT
    0x001800F0,    //         RFE_RAM.FSBWSETTLE       RFE_RAM.FSTXTONE
    0x00000000,    //         RFE_RAM.IFADCDLDOTX      RFE_RAM.IFADCDLDORX
    0x00000014     //         -                        RFE_RAM.DACTRIMADD
};

// Configuration length: MAIN
const uint32_t LRF_mainRegConfig_sunoqpsk_byteCount = sizeof(LRF_mainRegConfig_sunoqpsk);

// TRX_TxPowerLevels data structure
const TRX_TxPowerLevels TRX_txPowerLevels_sunoqpsk = {
    .numEntries  = 0x18,
    .powerLevels = {
        { .fraction = 0, .dBm = -30 },
        { .fraction = 0, .dBm = -20 },
        { .fraction = 0, .dBm = -15 },
        { .fraction = 0, .dBm = -10 },
        { .fraction = 0, .dBm =  -5 },
        { .fraction = 0, .dBm =  -4 },
        { .fraction = 0, .dBm =  -3 },
        { .fraction = 0, .dBm =  -2 },
        { .fraction = 0, .dBm =  -1 },
        { .fraction = 0, .dBm =   0 },
        { .fraction = 0, .dBm =   1 },
        { .fraction = 0, .dBm =   2 },
        { .fraction = 0, .dBm =   3 },
        { .fraction = 0, .dBm =   4 },
        { .fraction = 0, .dBm =   5 },
        { .fraction = 0, .dBm =   6 },
        { .fraction = 0, .dBm =   7 },
        { .fraction = 0, .dBm =   8 },
        { .fraction = 0, .dBm =   9 },
        { .fraction = 0, .dBm =  10 },
        { .fraction = 0, .dBm =  11 },
        { .fraction = 0, .dBm =  12 },
        { .fraction = 0, .dBm =  13 },
        { .fraction = 0, .dBm =  14 } 
    }
};

// TRX_TxPowerLevels data structure
const TRX_TxPowerLevels TRX_txPowerLevelsHigh_sunoqpsk = {
    .numEntries  = 0x13,
    .powerLevels = {
        { .fraction = 0, .dBm =   0 },
        { .fraction = 0, .dBm =   5 },
        { .fraction = 0, .dBm =  10 },
        { .fraction = 0, .dBm =  11 },
        { .fraction = 0, .dBm =  12 },
        { .fraction = 0, .dBm =  13 },
        { .fraction = 0, .dBm =  14 },
        { .fraction = 0, .dBm =  15 },
        { .fraction = 0, .dBm =  16 },
        { .fraction = 0, .dBm =  17 },
        { .fraction = 1, .dBm =  17 },
        { .fraction = 0, .dBm =  18 },
        { .fraction = 1, .dBm =  18 },
        { .fraction = 0, .dBm =  19 },
        { .fraction = 1, .dBm =  19 },
        { .fraction = 0, .dBm =  20 },
        { .fraction = 1, .dBm =  20 },
        { .fraction = 0, .dBm =  21 },
        { .fraction = 1, .dBm =  21 } 
    }
};
