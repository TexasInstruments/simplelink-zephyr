// SETTINGS FOR PHY BASED ON RADIO CONTROL LAYER (SOURCE FILE)
//
//
// CODE EXPORT INFORMATION
// This file is generated
//
// Tool name                             SmartRF Studio 8
// Tool version                          1.4.0.97
//
//
// WORKSPACE INFORMATION
//
// Workspace file                        tempws
// Device                                CC1407P
//     Package                           QFN40 5x5 RKP
//     Revision(s)                       A (1.0)
// SDK                                   SimpleLink CC1407 SDK 9.22.00.01
// Board                                 BP-EM-CC1407P-SE
// PHY                                   Wi-SUN multi-PHY code export
//     SUN FSK                           779 - 930 MHz - IEEE 802.15.4 - SUN FSK - Wi-SUN, Japan
//         State                         Experimental
//     SUN OFDM                          779 - 930 MHz - IEEE 802.15.4 - SUN OFDM - Wi-SUN, Japan
//         State                         Experimental
// PHY abbreviation                      wisun
//
//
// SUN FSK PHY PROPERTIES
//
// Wi-SUN multi-PHY code export:
//     Enabled FSK FEC modes in TX       Off, On
//     Enabled FSK operating modes       1a, 1b, 2a, 2b FAN, 3, 4a, 4b, 5
// Run-time properties:
//     FEC mode in TX                    Off
//     FSK operating mode                2b FAN (100 kbps, MI=1.0), FAN mode compliant preamble
//     Frequency                         920.600 MHz
//     TX output power                   14.0 dBm
//
//
// SUN OFDM PHY PROPERTIES
//
// Wi-SUN multi-PHY code export:
//     Enabled OFDM options              1, 2, 3, MCS0, 3, MCS1-6, 4, MCS0-1, 4, MCS2-6
// Run-time properties:
//     OFDM option                       3, MCS1-6
//     Modulation Coding Scheme in TX    MCS6
//         Data rate                     600 kbps
//     Frequency                         920.600 MHz
//     TX output power                   21.5 dBm (high output PA)

#include "rcl_settings_wisun.h"


// Configuration: FRONTEND
const uint32_t LRF_frontendRegConfig_wisun[] =
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
const uint32_t LRF_frontendRegConfig_wisun_byteCount = sizeof(LRF_frontendRegConfig_wisun);


// Configuration: MAIN
const uint32_t LRF_mainRegConfig_wisun[] =
{
    0x000081BB,    // 59 segment(s), total length = 443
    0x0000006F,    // Segment length = 111
    0x00349006,    //     Data structure 32-bit no-copy region (pointer byte position = 52, count = 7)
    0x002ECC15,    //         LRF_shapeBaseMce : scale, numCoeff
    0x07040201,    //         LRF_shapeBaseMce : coeff
    0x261C150F,    //         LRF_shapeBaseMce : -
    0x5D4D3E31,    //         LRF_shapeBaseMce : -
    0xAA96836F,    //         LRF_shapeBaseMce : -
    0xEADDCEBC,    //         LRF_shapeBaseMce : -
    0x00FFFBF4,    //         LRF_shapeBaseMce : -
    0x00088001,    //     Data structure zero region (start byte position = 8, count = 2)
    0x0018A001,    //     Data structure 32-bit region (start byte position = 24, count = 2)
    0x00000700,    //         TRX_swParam : dcDcSelection.dcDcTx.dtime, dcDcSelection.dcDcTx.ipeak, dcDcSelection.dcDcRx.dtime, dcDcSelection.dcDcRx.ipeak
    0x00000000,    //         TRX_swParam : sniffmodeTimeout
    0x002C9008,    //     Data structure 32-bit no-copy region (pointer byte position = 44, count = 9)
    0x00000008,    //         TRX_wisunMdrMappingTable : numEntries
    0x00021002,    //         TRX_wisunMdrMappingTable : entry
    0x00081204,    //         TRX_wisunMdrMappingTable : -
    0x00101205,    //         TRX_wisunMdrMappingTable : -
    0x00401407,    //         TRX_wisunMdrMappingTable : -
    0x00801408,    //         TRX_wisunMdrMappingTable : -
    0x02002730,    //         TRX_wisunMdrMappingTable : -
    0x08002340,    //         TRX_wisunMdrMappingTable : -
    0x20002150,    //         TRX_wisunMdrMappingTable : -
    0x00003010,    //     HW sparse region (address/value pairs, count = 17)
    0x1090AAAA,    //         PBE_FW.MODPREAMBLESHD
    0x209C0000,    //         LRFDMCE.OCESEND
    0x20B4001F,    //         LRFDMCE.MODSYMMAP1
    0x20C4000F,    //         LRFDMCE.BAUDPRE
    0x21800040,    //         LRFDMCE.DEMWMCFG
    0x21C81000,    //         LRFDMCE.MODCTRL
    0x30A40E00,    //         LRFDRFE.RX0CODC0
    0x319C0212,    //         LRFDRFE.RFECTL0
    0x31EC0000,    //         LRFDRFE.IFADC2
    0x32600000,    //         LRFDRFE.TXOFDMLDO
    0x3288D820,    //         LRFDRFE.MISC0
    0x32980C07,    //         LRFDRFE.PHEDISC
    0x33A42000,    //         RFE_FW.RXFRONTEND
    0x60BC0001,    //         LRFDOCE.MDM1
    0x60D82100,    //         LRFDOCE.SUNDEM1
    0x60E80835,    //         LRFDOCE.SUNCORR0
    0x61680100,    //         OCE_MAIN_FW.FWCFG4
    0x20CC1003,    //     HW 16-bit region (start address = 0x20CC, count = 4)
    0x00010001,    //         LRFDMCE.BAUDCOMP         LRFDMCE.DEMMISC0
    0x000148C4,    //         LRFDMCE.DEMMISC2         LRFDMCE.DEMMISC1
    0x20E81005,    //     HW 16-bit region (start address = 0x20E8, count = 6)
    0x1F9D0001,    //         LRFDMCE.DEMENSHDW1       LRFDMCE.DEMENSHDW0
    0x01100000,    //         LRFDMCE.DEMDSXB0         < GAP >
    0x00070054,    //         LRFDMCE.DEMFIFE0         LRFDMCE.DEMD2XB0
    0x21241004,    //     HW 16-bit region (start address = 0x2124, count = 5)
    0xAAAA7F00,    //         LRFDMCE.MDMSYNC0         LRFDMCE.DEMC1BE12
    0xAAAA7209,    //         LRFDMCE.MDMSYNC2         LRFDMCE.MDMSYNC1
    0x000072F6,    //         -                        LRFDMCE.MDMSYNC3
    0x21B01001,    //     HW 16-bit region (start address = 0x21B0, count = 2)
    0x00000001,    //         MCE_MAIN_FW.FWCFG2       MCE_SUNFSK_FW.FWCFG1
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
    0x33701007,    //     HW 16-bit region (start address = 0x3370, count = 8)
    0x76543210,    //         LRFDRFE.DCOHFT1          LRFDRFE.DCOHFT0
    0xFEDCBA98,    //         LRFDRFE.DCOHFT3          LRFDRFE.DCOHFT2
    0x76543210,    //         LRFDRFE.DCOHFB1          LRFDRFE.DCOHFB0
    0xFEDCBA98,    //         LRFDRFE.DCOHFB3          LRFDRFE.DCOHFB2
    0x33B80001,    //     HW zero region (start address = 0x33B8, count = 2)
    0x60F41001,    //     HW 16-bit region (start address = 0x60F4, count = 2)
    0x1F040800,    //         LRFDOCE.SUNPILOT0        LRFDOCE.SUNCORR3
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
    0x00000014,    //         -                        RFE_RAM.DACTRIMADD
    0x00007001,    //     RAM sparse region (address/value pairs, count = 2)
    0x80080013,    //         OCE_MAIN_RAM.SUNSTFTHR_LOW
    0x800C012C,    //         OCE_MAIN_RAM.SUNLTFTHR
    0x01004002,    // Segment length = 2 (FSK_MODE = 1A, 1B, 2A, 2B, 3, 4A, 4B, 5; OFDM_OPTION = 2, 3MCS0, 3MCS16, 4MCS01, 4MCS26)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x33AC0008,    //         RFE_FW.IFADCPEAKTHR
    0x00024002,    // Segment length = 2 (FSK_MODE = 1A, 2A, 2B, 3, 4A, 4B, 5; OFDM_OPTION = 1, 2, 3MCS0, 3MCS16, 4MCS01, 4MCS26)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x317C0092,    //         LRFDRFE.IQIC
    0x30004005,    // Segment length = 5 (FSK_MODE = 1A, 1B, 2A, 2B, 3, 4A, 4B, 5; OFDM_OPTION = 1, 2, 3MCS0, 3MCS16)
    0x00003003,    //     HW sparse region (address/value pairs, count = 4)
    0x30C8140F,    //         LRFDRFE.RX0MAGNCTRL0
    0x30F4140F,    //         LRFDRFE.RX1MAGNCTRL0
    0x30FC140F,    //         LRFDRFE.RX1MAGNCTRL2
    0x33C80055,    //         RFE_FW.AGCCFG8
    0x31004002,    // Segment length = 2 (FSK_MODE = 1A, 1B, 2A, 2B, 3, 4A, 4B, 5; OFDM_OPTION = 2, 3MCS0, 3MCS16)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x311C206A,    //         RFE_FW.AGCCFG3
    0x0F004002,    // Segment length = 2 (FSK_MODE = 1A, 1B, 2A, 2B, 3, 4A, 4B, 5; OFDM_OPTION = 4MCS01, 4MCS26)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x30CC163F,    //         LRFDRFE.RX0MAGNCTRL1
    0x3E004002,    // Segment length = 2 (FSK_MODE = 1A, 1B, 2A, 2B, 3, 4A, 4B, 5; OFDM_OPTION = 1)
    0x33941001,    //     HW 16-bit region (start address = 0x3394, count = 2)
    0x00470047,    //         RFE_FW.PHYRX1RSSI        RFE_FW.PHYRX0RSSI
    0x3F00400B,    // Segment length = 11 (FSK_MODE = 1A, 1B, 2A, 2B, 3, 4A, 4B, 5)
    0x0014A000,    //     Data structure 32-bit region (start byte position = 20, count = 1)
    0x00000009,    //         TRX_swParam : radioRequirements.defaultModem, radioRequirements.supportedRfModes
    0x00003007,    //     HW sparse region (address/value pairs, count = 8)
    0x115C0607,    //         PBE_FW.PKTCFG
    0x30640000,    //         LRFDRFE.TXENABLE
    0x30D0140F,    //         LRFDRFE.RX0MAGNCTRL2
    0x30F8163F,    //         LRFDRFE.RX1MAGNCTRL1
    0x31109EA3,    //         RFE_FW.AGCCFG0
    0x33905090,    //         LRFDRFE.SPARE0
    0x33A00090,    //         RFE_FW.RXCODCCFG2
    0x33C00000,    //         RFE_FW.AGCCFG6
    0x3F804002,    // Segment length = 2 (FSK_MODE = 1A, 1B, 2A, 2B, 3, 4A, 4B)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x33A80001,    //         RFE_FW.AGCCFG5
    0x00FC0003,    // Segment length = 3 (FSK_MODE = 2A, 2B, 3, 4A, 4B, 5)
    0x00003001,    //     HW sparse region (address/value pairs, count = 2)
    0x213C0000,    //         LRFDMCE.DEMCMIXFB2P
    0x21AC0000,    //         MCE_MAIN_FW.FWCFG0
    0x3F00000A,    // Segment length = 10 (OFDM_OPTION = 1, 2, 3MCS0, 3MCS16, 4MCS01, 4MCS26)
    0x0014A000,    //     Data structure 32-bit region (start byte position = 20, count = 1)
    0x0000000A,    //         TRX_swParam : radioRequirements.defaultModem, radioRequirements.supportedRfModes
    0x00003006,    //     HW sparse region (address/value pairs, count = 7)
    0x115C061F,    //         PBE_FW.PKTCFG
    0x30E40000,    //         LRFDRFE.RX1BDEC
    0x30F818FF,    //         LRFDRFE.RX1MAGNCTRL1
    0x311086A3,    //         RFE_FW.AGCCFG0
    0x31A00005,    //         LRFDRFE.RFECTL1
    0x33A000D0,    //         RFE_FW.RXCODCCFG2
    0x33C0001C,    //         RFE_FW.AGCCFG6
    0x3FE04003,    // Segment length = 3 (FSK_MODE = 1A, 1B, 2A, 2B, 3)
    0x00003001,    //     HW sparse region (address/value pairs, count = 2)
    0x10881F2F,    //         PBE_FW.MCECMDPAR0
    0x23A00002,    //         LRFDMCE.FWPRECTRL
    0x3F4A4003,    // Segment length = 3 (FSK_MODE = 1A, 2A, 3, 4A, 5)
    0x21001002,    //     HW 16-bit region (start address = 0x2100, count = 3)
    0x7C200400,    //         LRFDMCE.DEMMAFI1         LRFDMCE.DEMMAFI0
    0x000000C2,    //         -                        LRFDMCE.DEMMAFI2
    0x00EA0002,    // Segment length = 2 (FSK_MODE = 1B, 2B, 4A, 4B, 5)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x31186503,    //         RFE_FW.AGCCFG2
    0x00DC0002,    // Segment length = 2 (FSK_MODE = 2A, 2B, 3, 4B, 5)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x30E40040,    //         LRFDRFE.RX1BDEC
    0x00EC0003,    // Segment length = 3 (FSK_MODE = 2A, 2B, 4A, 4B, 5)
    0x00003001,    //     HW sparse region (address/value pairs, count = 2)
    0x20A40000,    //         LRFDMCE.CCESEND
    0x20BC0000,    //         LRFDMCE.BAUD0
    0x3D000002,    // Segment length = 2 (OFDM_OPTION = 1, 3MCS0, 3MCS16, 4MCS01, 4MCS26)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x30B80000,    //         LRFDRFE.RX0BDEC
    0x3E000002,    // Segment length = 2 (OFDM_OPTION = 2, 3MCS0, 3MCS16, 4MCS01, 4MCS26)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x33A80003,    //         RFE_FW.AGCCFG5
    0x3FF04002,    // Segment length = 2 (FSK_MODE = 1A, 1B, 2A, 2B)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x31A00081,    //         LRFDRFE.RFECTL1
    0x00F00002,    // Segment length = 2 (FSK_MODE = 3, 4A, 4B, 5)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x31A00001,    //         LRFDRFE.RFECTL1
    0x0F000005,    // Segment length = 5 (OFDM_OPTION = 1, 2, 3MCS0, 3MCS16)
    0x30CC1001,    //     HW 16-bit region (start address = 0x30CC, count = 2)
    0x18FF18FF,    //         LRFDRFE.RX0MAGNCTRL2     LRFDRFE.RX0MAGNCTRL1
    0x00003001,    //     HW sparse region (address/value pairs, count = 2)
    0x61AC0000,    //         OCE_MAIN_FW.FWCFG3
    0x62200000,    //         LRFDOCE.IFFTSAT
    0x3C000002,    // Segment length = 2 (OFDM_OPTION = 3MCS0, 3MCS16, 4MCS01, 4MCS26)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x339050B0,    //         LRFDRFE.SPARE0
    0x3FF84003,    // Segment length = 3 (FSK_MODE = 1A, 1B, 2A)
    0x31BCB001,    //     HW masked region (mask/value pairs, count = 2)
    0xFF0F4C0A,    //         LRFDRFE.LNA
    0x80FF00C6,    //         LRFDRFE.IFAMP
    0x3FEC4004,    // Segment length = 4 (FSK_MODE = 1A, 1B, 3)
    0x00003002,    //     HW sparse region (address/value pairs, count = 3)
    0x20A40080,    //         LRFDMCE.CCESEND
    0x20BC8000,    //         LRFDMCE.BAUD0
    0x339C0527,    //         RFE_FW.PHYAGCCFG4
    0x004A0003,    // Segment length = 3 (FSK_MODE = 1B, 2B, 4B)
    0x21001002,    //     HW 16-bit region (start address = 0x2100, count = 3)
    0x3E100200,    //         LRFDMCE.DEMMAFI1         LRFDMCE.DEMMAFI0
    0x00000061,    //         -                        LRFDMCE.DEMMAFI2
    0x008A0003,    // Segment length = 3 (FSK_MODE = 1B, 2B, 5)
    0x00003001,    //     HW sparse region (address/value pairs, count = 2)
    0x30C00015,    //         LRFDRFE.RX0CICHF
    0x30EC0815,    //         LRFDRFE.RX1CICHF
    0x001C0003,    // Segment length = 3 (FSK_MODE = 2A, 2B, 3)
    0x00003001,    //     HW sparse region (address/value pairs, count = 2)
    0x20680001,    //         LRFDMCE.RFCHANNEL
    0x20C00001,    //         LRFDMCE.BAUD1
    0x00380003,    // Segment length = 3 (FSK_MODE = 2B, 3, 4A)
    0x0000A001,    //     Data structure 32-bit region (start byte position = 0, count = 2)
    0x000493E0,    //         TRX_swParam : rxIntFrequency0
    0xFFFB6C20,    //         TRX_swParam : rxIntFrequency1
    0x30080003,    // Segment length = 3 (FSK_MODE = 2B; OFDM_OPTION = 4MCS01, 4MCS26)
    0x31BCB001,    //     HW masked region (mask/value pairs, count = 2)
    0xFF0F480A,    //         LRFDRFE.LNA
    0x80FF0086,    //         LRFDRFE.IFAMP
    0x00D00002,    // Segment length = 2 (FSK_MODE = 3, 4B, 5)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x30B80040,    //         LRFDRFE.RX0BDEC
    0x31000002,    // Segment length = 2 (OFDM_OPTION = 1, 4MCS01, 4MCS26)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x3118A8C6,    //         RFE_FW.AGCCFG2
    0x0E000002,    // Segment length = 2 (OFDM_OPTION = 2, 3MCS0, 3MCS16)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x611043AF,    //         LRFDOCE.SUNCORR4
    0x32000003,    // Segment length = 3 (OFDM_OPTION = 2, 4MCS01, 4MCS26)
    0x0000A001,    //     Data structure 32-bit region (start byte position = 0, count = 2)
    0x00061A80,    //         TRX_swParam : rxIntFrequency0
    0xFFF9E580,    //         TRX_swParam : rxIntFrequency1
    0x3FFC4006,    // Segment length = 6 (FSK_MODE = 1A, 1B)
    0x00003004,    //     HW sparse region (address/value pairs, count = 5)
    0x20680000,    //         LRFDMCE.RFCHANNEL
    0x20C00000,    //         LRFDMCE.BAUD1
    0x213C0002,    //         LRFDMCE.DEMCMIXFB2P
    0x30B80082,    //         LRFDRFE.RX0BDEC
    0x30E40080,    //         LRFDRFE.RX1BDEC
    0x3FFA4004,    // Segment length = 4 (FSK_MODE = 1A, 2A)
    0x00003002,    //     HW sparse region (address/value pairs, count = 3)
    0x30C0001D,    //         LRFDRFE.RX0CICHF
    0x30EC081D,    //         LRFDRFE.RX1CICHF
    0x31186403,    //         RFE_FW.AGCCFG2
    0x00060005,    // Segment length = 5 (FSK_MODE = 1B, 2A)
    0x0000A001,    //     Data structure 32-bit region (start byte position = 0, count = 2)
    0x000249F0,    //         TRX_swParam : rxIntFrequency0
    0xFFFDB610,    //         TRX_swParam : rxIntFrequency1
    0x0010A000,    //     Data structure 32-bit region (start byte position = 16, count = 1)
    0x000061A8,    //         TRX_swParam : modFrequencyDeviation
    0x000C0003,    // Segment length = 3 (FSK_MODE = 2A, 2B)
    0x00003001,    //     HW sparse region (address/value pairs, count = 2)
    0x30B80041,    //         LRFDRFE.RX0BDEC
    0x339C0528,    //         RFE_FW.PHYAGCCFG4
    0x00280002,    // Segment length = 2 (FSK_MODE = 2B, 4A)
    0x0010A000,    //     Data structure 32-bit region (start byte position = 16, count = 1)
    0x0000C350,    //         TRX_swParam : modFrequencyDeviation
    0x00300003,    // Segment length = 3 (FSK_MODE = 3, 4A)
    0x31BCB001,    //     HW masked region (mask/value pairs, count = 2)
    0xFF0F470A,    //         LRFDRFE.LNA
    0x80FF0076,    //         LRFDRFE.IFAMP
    0x00600006,    // Segment length = 6 (FSK_MODE = 4A, 4B)
    0x00003004,    //     HW sparse region (address/value pairs, count = 5)
    0x10881F4F,    //         PBE_FW.MCECMDPAR0
    0x20680002,    //         LRFDMCE.RFCHANNEL
    0x20C00002,    //         LRFDMCE.BAUD1
    0x23A00004,    //         LRFDMCE.FWPRECTRL
    0x339C0529,    //         RFE_FW.PHYAGCCFG4
    0x00C00006,    // Segment length = 6 (FSK_MODE = 4B, 5)
    0x0000A001,    //     Data structure 32-bit region (start byte position = 0, count = 2)
    0x0006DDD0,    //         TRX_swParam : rxIntFrequency0
    0xFFF92230,    //         TRX_swParam : rxIntFrequency1
    0x31BCB001,    //     HW masked region (mask/value pairs, count = 2)
    0xFF0F450A,    //         LRFDRFE.LNA
    0x80FF0056,    //         LRFDRFE.IFAMP
    0x01800002,    // Segment length = 2 (FSK_MODE = 5; OFDM_OPTION = 1)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x33A80002,    //         RFE_FW.AGCCFG5
    0x03000004,    // Segment length = 4 (OFDM_OPTION = 1, 2)
    0x00003002,    //     HW sparse region (address/value pairs, count = 3)
    0x30C00004,    //         LRFDRFE.RX0CICHF
    0x30EC0804,    //         LRFDRFE.RX1CICHF
    0x33903090,    //         LRFDRFE.SPARE0
    0x0C000014,    // Segment length = 20 (OFDM_OPTION = 3MCS0, 3MCS16)
    0x0000A001,    //     Data structure 32-bit region (start byte position = 0, count = 2)
    0x00086470,    //         TRX_swParam : rxIntFrequency0
    0xFFF79B90,    //         TRX_swParam : rxIntFrequency1
    0x00003007,    //     HW sparse region (address/value pairs, count = 8)
    0x11680201,    //         PBE_FW.OFDM0
    0x3064000E,    //         LRFDRFE.TXENABLE
    0x30C00055,    //         LRFDRFE.RX0CICHF
    0x30EC0855,    //         LRFDRFE.RX1CICHF
    0x311898C6,    //         RFE_FW.AGCCFG2
    0x60C4B826,    //         LRFDOCE.SUNFFT0
    0x60DC5006,    //         LRFDOCE.SUNDEM2
    0x60FC94D3,    //         LRFDOCE.SUNPILOT1
    0x31BCB001,    //     HW masked region (mask/value pairs, count = 2)
    0xFF0F430A,    //         LRFDRFE.LNA
    0x80FF0036,    //         LRFDRFE.IFAMP
    0x33941002,    //     HW 16-bit region (start address = 0x3394, count = 3)
    0x004C004C,    //         RFE_FW.PHYRX1RSSI        RFE_FW.PHYRX0RSSI
    0x0000122C,    //         -                        RFE_FW.PHYAGCCFG4
    0x60D01001,    //     HW 16-bit region (start address = 0x60D0, count = 2)
    0x3580DC17,    //         LRFDOCE.SUNDEM0          LRFDOCE.SUNMOD0
    0x30000012,    // Segment length = 18 (OFDM_OPTION = 4MCS01, 4MCS26)
    0x0000300C,    //     HW sparse region (address/value pairs, count = 13)
    0x11680301,    //         PBE_FW.OFDM0
    0x3064001E,    //         LRFDRFE.TXENABLE
    0x30C00065,    //         LRFDRFE.RX0CICHF
    0x30C81307,    //         LRFDRFE.RX0MAGNCTRL0
    0x30D0163F,    //         LRFDRFE.RX0MAGNCTRL2
    0x30EC0865,    //         LRFDRFE.RX1CICHF
    0x30F41307,    //         LRFDRFE.RX1MAGNCTRL0
    0x30FC18FF,    //         LRFDRFE.RX1MAGNCTRL2
    0x33C8004F,    //         RFE_FW.AGCCFG8
    0x60C4BC27,    //         LRFDOCE.SUNFFT0
    0x60DC5004,    //         LRFDOCE.SUNDEM2
    0x60FCD4D3,    //         LRFDOCE.SUNPILOT1
    0x61AC1F00,    //         OCE_MAIN_FW.FWCFG3
    0x33941001,    //     HW 16-bit region (start address = 0x3394, count = 2)
    0x004B004B,    //         RFE_FW.PHYRX1RSSI        RFE_FW.PHYRX0RSSI
    0x60D01001,    //     HW 16-bit region (start address = 0x60D0, count = 2)
    0x52006607,    //         LRFDOCE.SUNDEM0          LRFDOCE.SUNMOD0
    0x00014005,    // Segment length = 5 (FEC_MODE_IN_TX = OFF)
    0x00003003,    //     HW sparse region (address/value pairs, count = 4)
    0x118C01FF,    //         PBE_FW.TXPN9SEED
    0x21147F2D,    //         LRFDMCE.DEMC1BE1
    0x21207F2D,    //         LRFDMCE.DEMC1BE11
    0x21BC0000,    //         MCE_SUNFSK_FW.WISUNPHYMODE
    0x00010005,    // Segment length = 5 (FEC_MODE_IN_TX = ON)
    0x00003003,    //     HW sparse region (address/value pairs, count = 4)
    0x118C0000,    //         PBE_FW.TXPN9SEED
    0x21142D2D,    //         LRFDMCE.DEMC1BE1
    0x21202D2D,    //         LRFDMCE.DEMC1BE11
    0x21BC0010,    //         MCE_SUNFSK_FW.WISUNPHYMODE
    0x3FFE4007,    // Segment length = 7 (FSK_MODE = 1A)
    0x0000A001,    //     Data structure 32-bit region (start byte position = 0, count = 2)
    0x0002AB98,    //         TRX_swParam : rxIntFrequency0
    0xFFFD5468,    //         TRX_swParam : rxIntFrequency1
    0x0010A000,    //     Data structure 32-bit region (start byte position = 16, count = 1)
    0x000030D4,    //         TRX_swParam : modFrequencyDeviation
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x21AC0618,    //         MCE_MAIN_FW.FWCFG0
    0x00020003,    // Segment length = 3 (FSK_MODE = 1B)
    0x00003001,    //     HW sparse region (address/value pairs, count = 2)
    0x21AC2630,    //         MCE_MAIN_FW.FWCFG0
    0x317C005A,    //         LRFDRFE.IQIC
    0x00100006,    // Segment length = 6 (FSK_MODE = 3)
    0x0010A000,    //     Data structure 32-bit region (start byte position = 16, count = 1)
    0x0000927C,    //         TRX_swParam : modFrequencyDeviation
    0x00003002,    //     HW sparse region (address/value pairs, count = 3)
    0x30C0001C,    //         LRFDRFE.RX0CICHF
    0x30EC081C,    //         LRFDRFE.RX1CICHF
    0x31186504,    //         RFE_FW.AGCCFG2
    0x00200005,    // Segment length = 5 (FSK_MODE = 4A)
    0x00003003,    //     HW sparse region (address/value pairs, count = 4)
    0x30B80044,    //         LRFDRFE.RX0BDEC
    0x30C00011,    //         LRFDRFE.RX0CICHF
    0x30E40044,    //         LRFDRFE.RX1BDEC
    0x30EC0811,    //         LRFDRFE.RX1CICHF
    0x00400005,    // Segment length = 5 (FSK_MODE = 4B)
    0x0010A000,    //     Data structure 32-bit region (start byte position = 16, count = 1)
    0x000186A0,    //         TRX_swParam : modFrequencyDeviation
    0x00003001,    //     HW sparse region (address/value pairs, count = 2)
    0x30C00010,    //         LRFDRFE.RX0CICHF
    0x30EC0810,    //         LRFDRFE.RX1CICHF
    0x00800008,    // Segment length = 8 (FSK_MODE = 5)
    0x0010A000,    //     Data structure 32-bit region (start byte position = 16, count = 1)
    0x000124F8,    //         TRX_swParam : modFrequencyDeviation
    0x00003004,    //     HW sparse region (address/value pairs, count = 5)
    0x10881FAF,    //         PBE_FW.MCECMDPAR0
    0x20680003,    //         LRFDMCE.RFCHANNEL
    0x20C00003,    //         LRFDMCE.BAUD1
    0x23A0000A,    //         LRFDMCE.FWPRECTRL
    0x339C052A,    //         RFE_FW.PHYAGCCFG4
    0x01000013,    // Segment length = 19 (OFDM_OPTION = 1)
    0x0000A001,    //     Data structure 32-bit region (start byte position = 0, count = 2)
    0x000927C0,    //         TRX_swParam : rxIntFrequency0
    0xFFF6D840,    //         TRX_swParam : rxIntFrequency1
    0x00003009,    //     HW sparse region (address/value pairs, count = 10)
    0x11680001,    //         PBE_FW.OFDM0
    0x30640002,    //         LRFDRFE.TXENABLE
    0x311C2035,    //         RFE_FW.AGCCFG3
    0x339C242D,    //         RFE_FW.PHYAGCCFG4
    0x33AC000A,    //         RFE_FW.IFADCPEAKTHR
    0x60C4B038,    //         LRFDOCE.SUNFFT0
    0x60DC4006,    //         LRFDOCE.SUNDEM2
    0x60FC14D3,    //         LRFDOCE.SUNPILOT1
    0x6110432F,    //         LRFDOCE.SUNCORR4
    0x6178503C,    //         OCE_MAIN_FW.FWCFG2
    0x31BCB001,    //     HW masked region (mask/value pairs, count = 2)
    0xFF0F400A,    //         LRFDRFE.LNA
    0x80FF0006,    //         LRFDRFE.IFAMP
    0x60D01001,    //     HW 16-bit region (start address = 0x60D0, count = 2)
    0xC400D05F,    //         LRFDOCE.SUNDEM0          LRFDOCE.SUNMOD0
    0x02000011,    // Segment length = 17 (OFDM_OPTION = 2)
    0x00003007,    //     HW sparse region (address/value pairs, count = 8)
    0x11680101,    //         PBE_FW.OFDM0
    0x30640006,    //         LRFDRFE.TXENABLE
    0x30B80001,    //         LRFDRFE.RX0BDEC
    0x311899C6,    //         RFE_FW.AGCCFG2
    0x60C4A447,    //         LRFDOCE.SUNFFT0
    0x60DC4804,    //         LRFDOCE.SUNDEM2
    0x60FC54D3,    //         LRFDOCE.SUNPILOT1
    0x6178406C,    //         OCE_MAIN_FW.FWCFG2
    0x31BCB001,    //     HW masked region (mask/value pairs, count = 2)
    0xFF0F420A,    //         LRFDRFE.LNA
    0x80FF0026,    //         LRFDRFE.IFAMP
    0x33941002,    //     HW 16-bit region (start address = 0x3394, count = 3)
    0x00460046,    //         RFE_FW.PHYRX1RSSI        RFE_FW.PHYRX0RSSI
    0x00001228,    //         -                        RFE_FW.PHYAGCCFG4
    0x60D01001,    //     HW 16-bit region (start address = 0x60D0, count = 2)
    0x6900522F,    //         LRFDOCE.SUNDEM0          LRFDOCE.SUNMOD0
    0x04000002,    // Segment length = 2 (OFDM_OPTION = 3MCS0)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x617840CC,    //         OCE_MAIN_FW.FWCFG2
    0x08000002,    // Segment length = 2 (OFDM_OPTION = 3MCS16)
    0x00003000,    //     HW sparse region (address/value pairs, count = 1)
    0x6178406D,    //         OCE_MAIN_FW.FWCFG2
    0x10000005,    // Segment length = 5 (OFDM_OPTION = 4MCS01)
    0x00003003,    //     HW sparse region (address/value pairs, count = 4)
    0x311C213E,    //         RFE_FW.AGCCFG3
    0x339C1626,    //         RFE_FW.PHYAGCCFG4
    0x611043BA,    //         LRFDOCE.SUNCORR4
    0x6178518C,    //         OCE_MAIN_FW.FWCFG2
    0x20000005,    // Segment length = 5 (OFDM_OPTION = 4MCS26)
    0x00003003,    //     HW sparse region (address/value pairs, count = 4)
    0x311C20D4,    //         RFE_FW.AGCCFG3
    0x339C1629,    //         RFE_FW.PHYAGCCFG4
    0x611043B6,    //         LRFDOCE.SUNCORR4
    0x6178506E     //         OCE_MAIN_FW.FWCFG2
};

// Configuration length: MAIN
const uint32_t LRF_mainRegConfig_wisun_byteCount = sizeof(LRF_mainRegConfig_wisun);

// TRX_TxPowerLevels data structure
const TRX_TxPowerLevels TRX_txPowerLevels_wisun = {
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
const TRX_TxPowerLevels TRX_txPowerLevelsHigh_wisun = {
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
