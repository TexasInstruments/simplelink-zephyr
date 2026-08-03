// SETTINGS FOR PHY BASED ON RADIO CONTROL LAYER (SOURCE FILE)
//
//
// CODE EXPORT INFORMATION
// This file is generated
//
// Tool name                                 SmartRF Studio 8
// Tool version                              1.4.0.97
//
//
// WORKSPACE INFORMATION
//
// Workspace file                            tempws
// Device                                    CC1407P
//     Package                               QFN40 5x5 RKP
//     Revision(s)                           A (1.0)
// SDK                                       SimpleLink CC1407 SDK 9.22.00.01
// Board                                     BP-EM-CC1407P-SE
// PHY                                       779 - 930 MHz - Proprietary - 50 kbps, 25 kHz dev, 2-GFSK - Low power
//     State                                 Experimental
// PHY abbreviation                          2gfsk50kbps_lp
//
//
// PHY PROPERTIES
//
// Run-time properties:
//     Frequency                             915.000 MHz
//     TX output power                       14.0 dBm
//     Whitening                             Disabled
// RF channel:
//     Center frequency                      918.200 MHz 
//     Symbol rate                           50.00000 kBaud 
//     Deviation                             25.000 kHz 
//     RX filter bandwidth                   169.531 kHz 
//     Modulation                            2-GFSK 
// Start of packet:
//     Transmit start of packet              Enabled
//     Preamble count                        4 bytes
//     Preamble bit pattern                  10101010...
//     Synchronization word length           32 bits
//     Synchronization word                  0x930B51DE 
// Packet data:
//     Encoding                              None 
//         Data rate                         50.00000 kbps 
// Packet format:
//     Header length                         1 bytes
//     Packet/payload length                 Variable, 8-bit length field
//     Length field position in header       Bit 0 (MSBit of first byte)
//     Maximum payload length in RX          0 bytes
//     Header byte bit order                 Most significant bit first
//     Payload byte bit order                Most significant bit first
// End of packet:
//     Cyclic redundancy check (CRC) type    16-bit, CC1101 compatible
//     CRC includes packet header            Enabled

#include "rcl_settings_2gfsk50kbps_lp.h"


// Configuration: FRONTEND
const uint32_t LRF_frontendRegConfig_2gfsk50kbpsLp[] =
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
const uint32_t LRF_frontendRegConfig_2gfsk50kbpsLp_byteCount = sizeof(LRF_frontendRegConfig_2gfsk50kbpsLp);


// Configuration: MAIN
const uint32_t LRF_mainRegConfig_2gfsk50kbpsLp[] =
{
    0x000080A7,    // 5 segment(s), total length = 167
    0x00000092,    // Segment length = 146
    0x00349005,    //     Data structure 32-bit no-copy region (pointer byte position = 52, count = 6)
    0x0022FDE2,    //         LRF_shapeBaseGfsk067 : scale, numCoeff
    0x0A050201,    //         LRF_shapeBaseGfsk067 : coeff
    0x52372214,    //         LRF_shapeBaseGfsk067 : -
    0xCBB09171,    //         LRF_shapeBaseGfsk067 : -
    0xFDF8EEE0,    //         LRF_shapeBaseGfsk067 : -
    0x000000FF,    //         LRF_shapeBaseGfsk067 : -
    0x0000A007,    //     Data structure 32-bit region (start byte position = 0, count = 8)
    0x0002AB98,    //         TRX_swParam : rxIntFrequency0
    0xFFFD5468,    //         TRX_swParam : rxIntFrequency1
    0x00000000,    //         TRX_swParam : rxFrequencyOffset
    0x00000000,    //         TRX_swParam : txFrequencyOffset
    0x000061A8,    //         TRX_swParam : modFrequencyDeviation
    0x00000011,    //         TRX_swParam : radioRequirements.defaultModem, radioRequirements.supportedRfModes
    0x00000700,    //         TRX_swParam : dcDcSelection.dcDcTx.dtime, dcDcSelection.dcDcTx.ipeak, dcDcSelection.dcDcRx.dtime, dcDcSelection.dcDcRx.ipeak
    0x00000000,    //         TRX_swParam : sniffmodeTimeout
    0x0000301B,    //     HW sparse region (address/value pairs, count = 28)
    0x10600000,    //         PBE_FW.CMDPAR1
    0x10881F37,    //         PBE_FW.MCECMDPAR0
    0x10905555,    //         PBE_FW.MODPREAMBLESHD
    0x11640083,    //         PBE_FW.FSKCFG
    0x12100007,    //         LRFDPBE.LFSR0N
    0x12200007,    //         LRFDPBE.LFSR1N
    0x20680000,    //         LRFDMCE.RFCHANNEL
    0x209C0000,    //         LRFDMCE.OCESEND
    0x20A40080,    //         LRFDMCE.CCESEND
    0x20B4001F,    //         LRFDMCE.MODSYMMAP1
    0x213C0002,    //         LRFDMCE.DEMCMIXFB2P
    0x21AC0A30,    //         MCE_MAIN_FW.FWCFG0
    0x21B40000,    //         MCE_MAIN_FW.FWCFG2
    0x23A00003,    //         LRFDMCE.FWPRECTRL
    0x30640000,    //         LRFDRFE.TXENABLE
    0x30880000,    //         LRFDRFE.RXADCDIG
    0x30A40E00,    //         LRFDRFE.RX0CODC0
    0x30B80081,    //         LRFDRFE.RX0BDEC
    0x30C00015,    //         LRFDRFE.RX0CICHF
    0x30E40080,    //         LRFDRFE.RX1BDEC
    0x30EC0015,    //         LRFDRFE.RX1CICHF
    0x311082A3,    //         RFE_FW.AGCCFG0
    0x317C005A,    //         LRFDRFE.IQIC
    0x31EC0000,    //         LRFDRFE.IFADC2
    0x32600000,    //         LRFDRFE.TXOFDMLDO
    0x3288D820,    //         LRFDRFE.MISC0
    0x32980C07,    //         LRFDRFE.PHEDISC
    0x33C80055,    //         RFE_FW.AGCCFG8
    0x20BC1002,    //     HW 16-bit region (start address = 0x20BC, count = 3)
    0x00008000,    //         LRFDMCE.BAUD1            LRFDMCE.BAUD0
    0x0000000F,    //         -                        LRFDMCE.BAUDPRE
    0x20CC1003,    //     HW 16-bit region (start address = 0x20CC, count = 4)
    0x00010001,    //         LRFDMCE.BAUDCOMP         LRFDMCE.DEMMISC0
    0x000148C4,    //         LRFDMCE.DEMMISC2         LRFDMCE.DEMMISC1
    0x20E81008,    //     HW 16-bit region (start address = 0x20E8, count = 9)
    0x1F9D0001,    //         LRFDMCE.DEMENSHDW1       LRFDMCE.DEMENSHDW0
    0x01100000,    //         LRFDMCE.DEMDSXB0         < GAP >
    0x00050054,    //         LRFDMCE.DEMFIFE0         LRFDMCE.DEMD2XB0
    0x3E100200,    //         LRFDMCE.DEMMAFI1         LRFDMCE.DEMMAFI0
    0x00000061,    //         -                        LRFDMCE.DEMMAFI2
    0x21101009,    //     HW 16-bit region (start address = 0x2110, count = 10)
    0x7F270020,    //         LRFDMCE.DEMC1BE1         LRFDMCE.DEMC1BE0
    0x00C30100,    //         LRFDMCE.DEMC1BE10        LRFDMCE.DEMC1BE2
    0x7F007F0C,    //         LRFDMCE.DEMC1BE12        LRFDMCE.DEMC1BE11
    0x7B8AD0C9,    //         LRFDMCE.MDMSYNC1         LRFDMCE.MDMSYNC0
    0x705E0000,    //         LRFDMCE.MDMSYNC3         LRFDMCE.MDMSYNC2
    0x21681001,    //     HW 16-bit region (start address = 0x2168, count = 2)
    0xAAAAAAAA,    //         LRFDMCE.DEMC1BEREF5      LRFDMCE.DEMC1BEREF4
    0x21801002,    //     HW 16-bit region (start address = 0x2180, count = 3)
    0x00020040,    //         LRFDMCE.DEMWMPRE         LRFDMCE.DEMWMCFG
    0x00000008,    //         -                        LRFDMCE.DEMWMPTHR
    0x21C41001,    //     HW 16-bit region (start address = 0x21C4, count = 2)
    0x10000000,    //         LRFDMCE.MODCTRL          MCE_MAIN_FW.FSKMODE
    0x30C81002,    //     HW 16-bit region (start address = 0x30C8, count = 3)
    0x163F140F,    //         LRFDRFE.RX0MAGNCTRL1     LRFDRFE.RX0MAGNCTRL0
    0x0000140F,    //         -                        LRFDRFE.RX0MAGNCTRL2
    0x30F41002,    //     HW 16-bit region (start address = 0x30F4, count = 3)
    0x163F140F,    //         LRFDRFE.RX1MAGNCTRL1     LRFDRFE.RX1MAGNCTRL0
    0x0000140F,    //         -                        LRFDRFE.RX1MAGNCTRL2
    0x31181001,    //     HW 16-bit region (start address = 0x3118, count = 2)
    0x206A9205,    //         RFE_FW.AGCCFG3           RFE_FW.AGCCFG2
    0x319C1001,    //     HW 16-bit region (start address = 0x319C, count = 2)
    0x00810212,    //         LRFDRFE.RFECTL1          LRFDRFE.RFECTL0
    0x31BC1001,    //     HW 16-bit region (start address = 0x31BC, count = 2)
    0x00C44C08,    //         LRFDRFE.IFAMP            LRFDRFE.LNA
    0x31C81007,    //     HW 16-bit region (start address = 0x31C8, count = 8)
    0x02060084,    //         LRFDRFE.ULNA             LRFDRFE.PA
    0x00000000,    //         LRFDRFE.IFADC0           LRFDRFE.RFLDO
    0x00000000,    //         LRFDRFE.IFADCLF          LRFDRFE.IFADC1
    0x0000C000,    //         LRFDRFE.IFADCALDO        LRFDRFE.IFADCQUANT
    0x31F40002,    //     HW zero region (start address = 0x31F4, count = 3)
    0x32001016,    //     HW 16-bit region (start address = 0x3200, count = 23)
    0x01040C00,    //         LRFDRFE.DIV              LRFDRFE.DIVCTL
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
    0x00007000,    //     RAM sparse region (address/value pairs, count = 1)
    0x00040000,    //         PBE_RAM.MCEPOLY0L
    0x00085002,    //     RAM 16-bit region (start address = 0x0008, count = 3)
    0x80050000,    //         PBE_RAM.MCEPOLY1H        PBE_RAM.MCEPOLY1L
    0x00000000,    //         -                        PBE_RAM.MCELFSR0INITL
    0x00106002,    //     RAM 32-bit region (start address = 0x0010, count = 3)
    0xFFFF0000,    //         PBE_RAM.MCELFSR1INITH    PBE_RAM.MCELFSR1INITL
    0x00000000,    //         PBE_RAM.MCECRCXORH       PBE_RAM.MCECRCXORL
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
    0x00000014,    //         -                        RFE_RAM.DACTRIMADD
    0x00014006,    // Segment length = 6 (WHITENING = DISABLED)
    0x00003001,    //     HW sparse region (address/value pairs, count = 2)
    0x115C1328,    //         PBE_FW.PKTCFG
    0x118C01FF,    //         PBE_FW.TXPN9SEED
    0x00007001,    //     RAM sparse region (address/value pairs, count = 2)
    0x00068005,    //         PBE_RAM.MCEPOLY0H
    0x000EFFFF,    //         PBE_RAM.MCELFSR0INITH
    0x00010006,    // Segment length = 6 (WHITENING = CC1101_COMPATIBLE)
    0x00003001,    //     HW sparse region (address/value pairs, count = 2)
    0x115C376E,    //         PBE_FW.PKTCFG
    0x118C000F,    //         PBE_FW.TXPN9SEED
    0x00007001,    //     RAM sparse region (address/value pairs, count = 2)
    0x00061080,    //         PBE_RAM.MCEPOLY0H
    0x000E000F,    //         PBE_RAM.MCELFSR0INITH
    0x00024002,    // Segment length = 2 (TX_SYNC_WORD = A)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x21BC0000,    //         MCE_MAIN_FW.FWCFG1
    0x00020002,    // Segment length = 2 (TX_SYNC_WORD = B)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x21BC0001     //         MCE_MAIN_FW.FWCFG1
};

// Configuration length: MAIN
const uint32_t LRF_mainRegConfig_2gfsk50kbpsLp_byteCount = sizeof(LRF_mainRegConfig_2gfsk50kbpsLp);

// TRX_TxPowerLevels data structure
const TRX_TxPowerLevels TRX_txPowerLevels_2gfsk50kbpsLp = {
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
const TRX_TxPowerLevels TRX_txPowerLevelsHigh_2gfsk50kbpsLp = {
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
