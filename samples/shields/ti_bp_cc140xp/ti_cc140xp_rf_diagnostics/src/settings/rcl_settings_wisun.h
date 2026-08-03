// SETTINGS FOR PHY BASED ON RADIO CONTROL LAYER (HEADER FILE)
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

#ifndef RCLSETTINGSWISUN_H
#define RCLSETTINGSWISUN_H

#include <stdint.h>
#include <ti_cc140xp/ti_cc140xp.h>


// PHY features
#define TRX_PHY_FEATURE_FEC_MODE_IN_TX_OFF_WISUN    0x0000
#define TRX_PHY_FEATURE_FEC_MODE_IN_TX_ON_WISUN     0x0001
#define TRX_PHY_FEATURE_FSK_MODE_1A_WISUN           0x0000
#define TRX_PHY_FEATURE_FSK_MODE_1B_WISUN           0x0002
#define TRX_PHY_FEATURE_FSK_MODE_2A_WISUN           0x0004
#define TRX_PHY_FEATURE_FSK_MODE_2B_WISUN           0x0008
#define TRX_PHY_FEATURE_FSK_MODE_3_WISUN            0x0010
#define TRX_PHY_FEATURE_FSK_MODE_4A_WISUN           0x0020
#define TRX_PHY_FEATURE_FSK_MODE_4B_WISUN           0x0040
#define TRX_PHY_FEATURE_FSK_MODE_5_WISUN            0x0080
#define TRX_PHY_FEATURE_OFDM_OPTION_1_WISUN         0x0100
#define TRX_PHY_FEATURE_OFDM_OPTION_2_WISUN         0x0200
#define TRX_PHY_FEATURE_OFDM_OPTION_3MCS0_WISUN     0x0400
#define TRX_PHY_FEATURE_OFDM_OPTION_3MCS16_WISUN    0x0800
#define TRX_PHY_FEATURE_OFDM_OPTION_4MCS01_WISUN    0x1000
#define TRX_PHY_FEATURE_OFDM_OPTION_4MCS26_WISUN    0x2000


// Supported clock configuration
#define TRX_CLOCK_CONFIG_WISUN                      TRX_ClockConfig_XOSC

// Supported RF mode
#define TRX_RF_MODE_WISUN                           TRX_RfMode_SUN


// Configuration: FRONTEND
extern const uint32_t LRF_frontendRegConfig_wisun[];
// Configuration length: FRONTEND
extern const uint32_t LRF_frontendRegConfig_wisun_byteCount;
// Configuration: MAIN
extern const uint32_t LRF_mainRegConfig_wisun[];
// Configuration length: MAIN
extern const uint32_t LRF_mainRegConfig_wisun_byteCount;


// TRX data structures
extern const TRX_TxPowerLevels	TRX_txPowerLevels_wisun;
extern const TRX_TxPowerLevels	TRX_txPowerLevelsHigh_wisun;


// MAIN : FEC MODE IN TX register field values

// Address    Module           Register              Bit(s)     Field                 Value (OFF)    Value (ON)
// ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// 0x118C     PBE_FW           TXPN9SEED             [8:0]      VAL                   0x1FF          0x000
// 0x2114     LRFDMCE          DEMC1BE1              [15:8]     THRESHOLDB            0x7F           0x2D
// 0x2114     LRFDMCE          DEMC1BE1              [7:0]      THRESHOLDA            0x2D           0x2D
// 0x2120     LRFDMCE          DEMC1BE11             [15:8]     THRESHOLDE            0x7F           0x2D
// 0x2120     LRFDMCE          DEMC1BE11             [7:0]      THRESHOLDD            0x2D           0x2D
// 0x21BC     MCE_SUNFSK_FW    WISUNPHYMODE          [7:4]      TYPE                  0x0            0x1


// MAIN : FSK MODE register field values

// Address    Module           Register              Bit(s)     Field                 Value (1A)     Value (1B)    Value (2A)       Value (2B)        Value (3)         Value (4A)    Value (4B)    Value (5)
// ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// 0x1088     PBE_FW           MCECMDPAR0            [15]       INFPREAMBLE           0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x1088     PBE_FW           MCECMDPAR0            [13:8]     SYNCWORDLEN           0x1F           0x1F          0x1F             0x1F              0x1F              0x1F          0x1F          0x1F
// 0x1088     PBE_FW           MCECMDPAR0            [7:4]      PREAMBLEREPS          0x2            0x2           0x2              0x2               0x2               0x4           0x4           0xA
// 0x1088     PBE_FW           MCECMDPAR0            [3:0]      PREAMBLESIZE          0xF            0xF           0xF              0xF               0xF               0xF           0xF           0xF
// 0x1090     PBE_FW           MODPREAMBLESHD        [15:0]     PREAMBLEPAT           0xAAAA         0xAAAA        0xAAAA           0xAAAA            0xAAAA            0xAAAA        0xAAAA        0xAAAA
// 0x115C     PBE_FW           PKTCFG                [12]       DATABITREV            0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x115C     PBE_FW           PKTCFG                [11]       DATAWHBEFORECRC       0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x115C     PBE_FW           PKTCFG                [10]       DATAWHEN              0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x115C     PBE_FW           PKTCFG                [9]        DATACRCEN             0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x115C     PBE_FW           PKTCFG                [2]        WHCRCBITS             0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x115C     PBE_FW           PKTCFG                [1]        WHITENER              0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x115C     PBE_FW           PKTCFG                [0]        SUNMODE               0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x2068     LRFDMCE          RFCHANNEL             [11:0]     VAL                   0x000          0x000         0x001            0x001             0x001             0x002         0x002         0x003
// 0x209C     LRFDMCE          OCESEND               [7:0]      VAL                   0x00           0x00          0x00             0x00              0x00              0x00          0x00          0x00
// 0x20A4     LRFDMCE          CCESEND               [7:0]      VAL                   0x80           0x80          0x00             0x00              0x80              0x00          0x00          0x00
// 0x20B4     LRFDMCE          MODSYMMAP1            [7:4]      SYM5                  0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20B4     LRFDMCE          MODSYMMAP1            [3:0]      SYM4                  0xF            0xF           0xF              0xF               0xF               0xF           0xF           0xF
// 0x20BC     LRFDMCE          BAUD0                 [15:0]     RATEWORD              0x8000         0x8000        0x0000           0x0000            0x8000            0x0000        0x0000        0x0000
// 0x20C0     LRFDMCE          BAUD1                 [4:0]      EXTRATEWORD           0x00           0x00          0x01             0x01              0x01              0x02          0x02          0x03
// 0x20C4     LRFDMCE          BAUDPRE               [7:0]      PRESCALER             0x0F           0x0F          0x0F             0x0F              0x0F              0x0F          0x0F          0x0F
// 0x20CC     LRFDMCE          DEMMISC0              [1:0]      RXFRONTSEL            0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20D0     LRFDMCE          BAUDCOMP              [1:0]      SCALEVAL              0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20D4     LRFDMCE          DEMMISC1              [15:14]    LQIPERIOD             0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20D4     LRFDMCE          DEMMISC1              [12:11]    MAFCGAIN              0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20D4     LRFDMCE          DEMMISC1              [9:7]      STIMTEAPERIOD         0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20D4     LRFDMCE          DEMMISC1              [6:4]      STIMTEAGAIN           0x4            0x4           0x4              0x4               0x4               0x4           0x4           0x4
// 0x20D4     LRFDMCE          DEMMISC1              [2]        PDIFDESPECKLEREN      0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20D4     LRFDMCE          DEMMISC1              [0]        PDIFLIMITRANGE        0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20D8     LRFDMCE          DEMMISC2              [1:0]      PDIFDECIM             0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20E8     LRFDMCE          DEMENSHDW0            [3]        WAVEMATCH             0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20E8     LRFDMCE          DEMENSHDW0            [2]        FE13                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20E8     LRFDMCE          DEMENSHDW0            [1]        THRD                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20E8     LRFDMCE          DEMENSHDW0            [0]        FRAC                  0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20EC     LRFDMCE          DEMENSHDW1            [15]       VITE                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20EC     LRFDMCE          DEMENSHDW1            [14]       MLSE                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20EC     LRFDMCE          DEMENSHDW1            [13]       SOFD                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20EC     LRFDMCE          DEMENSHDW1            [12]       SWQU                  0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20EC     LRFDMCE          DEMENSHDW1            [11]       MAFC                  0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20EC     LRFDMCE          DEMENSHDW1            [10]       MAFI                  0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20EC     LRFDMCE          DEMENSHDW1            [9]        FIFE                  0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20EC     LRFDMCE          DEMENSHDW1            [8]        PDIF                  0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20EC     LRFDMCE          DEMENSHDW1            [7]        CA2P                  0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20EC     LRFDMCE          DEMENSHDW1            [6]        FECP                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20EC     LRFDMCE          DEMENSHDW1            [5]        FEC5                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20EC     LRFDMCE          DEMENSHDW1            [4]        C1BE                  0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20EC     LRFDMCE          DEMENSHDW1            [3]        LQIE                  0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20EC     LRFDMCE          DEMENSHDW1            [2]        F4BA                  0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20EC     LRFDMCE          DEMENSHDW1            [1]        STIM                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20EC     LRFDMCE          DEMENSHDW1            [0]        DSBU                  0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20F4     LRFDMCE          DEMDSXB0              [11:10]    OUTSRCSEL2            0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20F4     LRFDMCE          DEMDSXB0              [9:8]      OUTSRCSEL1            0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20F4     LRFDMCE          DEMDSXB0              [7:6]      B4SRCSEL              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20F4     LRFDMCE          DEMDSXB0              [5:4]      B3SRCSEL              0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20F4     LRFDMCE          DEMDSXB0              [3:2]      B2SRCSEL              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20F4     LRFDMCE          DEMDSXB0              [1:0]      B1SRCSEL              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20F8     LRFDMCE          DEMD2XB0              [7:6]      OUTSRCSEL2            0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20F8     LRFDMCE          DEMD2XB0              [5:4]      OUTSRCSEL1            0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20F8     LRFDMCE          DEMD2XB0              [3:2]      B2SRCSEL              0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x20F8     LRFDMCE          DEMD2XB0              [1:0]      B1SRCSEL              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20FC     LRFDMCE          DEMFIFE0              [11]       FINEFOESEL            0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20FC     LRFDMCE          DEMFIFE0              [10:9]     FOCFFSEL              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20FC     LRFDMCE          DEMFIFE0              [8]        ACCCNTMODE            0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20FC     LRFDMCE          DEMFIFE0              [7:6]      ACCPERIOD             0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20FC     LRFDMCE          DEMFIFE0              [5]        ACCEN                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20FC     LRFDMCE          DEMFIFE0              [4]        IIRUSEINITIAL         0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x20FC     LRFDMCE          DEMFIFE0              [3:1]      IIRGAIN               0x3            0x3           0x3              0x3               0x3               0x3           0x3           0x3
// 0x20FC     LRFDMCE          DEMFIFE0              [0]        IIREN                 0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x2100     LRFDMCE          DEMMAFI0              [15:8]     C1C7                  0x04           0x02          0x04             0x02              0x04              0x04          0x02          0x04
// 0x2100     LRFDMCE          DEMMAFI0              [7:0]      C0C8                  0x00           0x00          0x00             0x00              0x00              0x00          0x00          0x00
// 0x2104     LRFDMCE          DEMMAFI1              [15:8]     C3C5                  0x7C           0x3E          0x7C             0x3E              0x7C              0x7C          0x3E          0x7C
// 0x2104     LRFDMCE          DEMMAFI1              [7:0]      C2C6                  0x20           0x10          0x20             0x10              0x20              0x20          0x10          0x20
// 0x2108     LRFDMCE          DEMMAFI2              [8:0]      C4                    0x0C2          0x061         0x0C2            0x061             0x0C2             0x0C2         0x061         0x0C2
// 0x2124     LRFDMCE          DEMC1BE12             [15:8]     THRESHOLDG            0x7F           0x7F          0x7F             0x7F              0x7F              0x7F          0x7F          0x7F
// 0x2128     LRFDMCE          MDMSYNC0              [15:0]     SWA15C0               0xAAAA         0xAAAA        0xAAAA           0xAAAA            0xAAAA            0xAAAA        0xAAAA        0xAAAA
// 0x212C     LRFDMCE          MDMSYNC1              [15:0]     SWA31C16              0x7209         0x7209        0x7209           0x7209            0x7209            0x7209        0x7209        0x7209
// 0x2130     LRFDMCE          MDMSYNC2              [15:0]     SWB15C0               0xAAAA         0xAAAA        0xAAAA           0xAAAA            0xAAAA            0xAAAA        0xAAAA        0xAAAA
// 0x2134     LRFDMCE          MDMSYNC3              [15:0]     SWB31C16              0x72F6         0x72F6        0x72F6           0x72F6            0x72F6            0x72F6        0x72F6        0x72F6
// 0x213C     LRFDMCE          DEMCMIXFB2P           [1:0]      CTL                   0x2            0x2           0x0              0x0               0x0               0x0           0x0           0x0
// 0x2180     LRFDMCE          DEMWMCFG              [7:6]      SCALING               0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x21AC     MCE_MAIN_FW      FWCFG0                [15]       FB2PLLUPDRATE         0x0            0x0           -                -                 -                 -             -             -
// 0x21AC     MCE_MAIN_FW      FWCFG0                [13]       FB2PLLEN              0x0            0x1           0x0              0x0               0x0               0x0           0x0           0x0
// 0x21AC     MCE_MAIN_FW      FWCFG0                [11:8]     FB2PLLGAIN            0x6            0x6           0x0              0x0               0x0               0x0           0x0           0x0
// 0x21AC     MCE_MAIN_FW      FWCFG0                [7:0]      FB2PLLLIMIT           0x18           0x30          0x00             0x00              0x00              0x00          0x00          0x00
// 0x21B0     MCE_SUNFSK_FW    FWCFG1                [0]        BITREP                0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x21B4     MCE_MAIN_FW      FWCFG2                [15]       PQTEN                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x21C8     LRFDMCE          MODCTRL               [12]       CDCCOLRST             0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x21C8     LRFDMCE          MODCTRL               [11]       DSBUSEL               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x23A0     LRFDMCE          FWPRECTRL             [15:0]     SWREPS                0x0002         0x0002        0x0002           0x0002            0x0002            0x0004        0x0004        0x000A
// 0x3064     LRFDRFE          TXENABLE              [4]        INTP4                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3064     LRFDRFE          TXENABLE              [3]        INTP3                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3064     LRFDRFE          TXENABLE              [2]        INTP2                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3064     LRFDRFE          TXENABLE              [1]        INTP1                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3064     LRFDRFE          TXENABLE              [0]        FRONTEND              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x30A4     LRFDRFE          RX0CODC0              [11]       ESTSEL                0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x30A4     LRFDRFE          RX0CODC0              [10:9]     COMPSEL               0x3            0x3           0x3              0x3               0x3               0x3           0x3           0x3
// 0x30A4     LRFDRFE          RX0CODC0              [8]        IIRUSEINITIAL         0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x30B8     LRFDRFE          RX0BDEC               [7:6]      BDE2DVGA              0x2            0x2           0x1              0x1               0x1               0x1           0x1           0x1
// 0x30B8     LRFDRFE          RX0BDEC               [5:4]      BDE1DVGA              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x30B8     LRFDRFE          RX0BDEC               [3:2]      BDEC2NUMSTAGES        0x0            0x0           0x0              0x0               0x0               0x1           0x0           0x0
// 0x30B8     LRFDRFE          RX0BDEC               [1:0]      BDEC1NUMSTAGES        0x2            0x2           0x1              0x1               0x0               0x0           0x0           0x0
// 0x30C0     LRFDRFE          RX0CICHF              [5:2]      CICDEC                0x7            0x5           0x7              0x5               0x7               0x4           0x4           0x5
// 0x30C0     LRFDRFE          RX0CICHF              [1:0]      CHFIBW                0x1            0x1           0x1              0x1               0x0               0x1           0x0           0x1
// 0x30C8     LRFDRFE          RX0MAGNCTRL0          [12]       PERMODE               0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x30C8     LRFDRFE          RX0MAGNCTRL0          [11:8]     SCALE                 0x4            0x4           0x4              0x4               0x4               0x4           0x4           0x4
// 0x30C8     LRFDRFE          RX0MAGNCTRL0          [7:0]      PERIOD                0x0F           0x0F          0x0F             0x0F              0x0F              0x0F          0x0F          0x0F
// 0x30CC     LRFDRFE          RX0MAGNCTRL1          [12]       PERMODE               0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x30CC     LRFDRFE          RX0MAGNCTRL1          [11:8]     SCALE                 0x6            0x6           0x6              0x6               0x6               0x6           0x6           0x6
// 0x30CC     LRFDRFE          RX0MAGNCTRL1          [7:0]      PERIOD                0x3F           0x3F          0x3F             0x3F              0x3F              0x3F          0x3F          0x3F
// 0x30D0     LRFDRFE          RX0MAGNCTRL2          [12]       PERMODE               0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x30D0     LRFDRFE          RX0MAGNCTRL2          [11:8]     SCALE                 0x4            0x4           0x4              0x4               0x4               0x4           0x4           0x4
// 0x30D0     LRFDRFE          RX0MAGNCTRL2          [7:0]      PERIOD                0x0F           0x0F          0x0F             0x0F              0x0F              0x0F          0x0F          0x0F
// 0x30E4     LRFDRFE          RX1BDEC               [7:6]      BDE2DVGA              0x2            0x2           0x1              0x1               0x1               0x1           0x1           0x1
// 0x30E4     LRFDRFE          RX1BDEC               [3:2]      BDEC2NUMSTAGES        0x0            0x0           0x0              0x0               0x0               0x1           0x0           0x0
// 0x30EC     LRFDRFE          RX1CICHF              [11:10]    ACC1MGE2SRCSEL        0x2            0x2           0x2              0x2               0x2               0x2           0x2           0x2
// 0x30EC     LRFDRFE          RX1CICHF              [5:2]      CICDEC                0x7            0x5           0x7              0x5               0x7               0x4           0x4           0x5
// 0x30EC     LRFDRFE          RX1CICHF              [1:0]      CHFIBW                0x1            0x1           0x1              0x1               0x0               0x1           0x0           0x1
// 0x30F4     LRFDRFE          RX1MAGNCTRL0          [12]       PERMODE               0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x30F4     LRFDRFE          RX1MAGNCTRL0          [11:8]     SCALE                 0x4            0x4           0x4              0x4               0x4               0x4           0x4           0x4
// 0x30F4     LRFDRFE          RX1MAGNCTRL0          [7:0]      PERIOD                0x0F           0x0F          0x0F             0x0F              0x0F              0x0F          0x0F          0x0F
// 0x30F8     LRFDRFE          RX1MAGNCTRL1          [12]       PERMODE               0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x30F8     LRFDRFE          RX1MAGNCTRL1          [11:8]     SCALE                 0x6            0x6           0x6              0x6               0x6               0x6           0x6           0x6
// 0x30F8     LRFDRFE          RX1MAGNCTRL1          [7:0]      PERIOD                0x3F           0x3F          0x3F             0x3F              0x3F              0x3F          0x3F          0x3F
// 0x30FC     LRFDRFE          RX1MAGNCTRL2          [12]       PERMODE               0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x30FC     LRFDRFE          RX1MAGNCTRL2          [11:8]     SCALE                 0x4            0x4           0x4              0x4               0x4               0x4           0x4           0x4
// 0x30FC     LRFDRFE          RX1MAGNCTRL2          [7:0]      PERIOD                0x0F           0x0F          0x0F             0x0F              0x0F              0x0F          0x0F          0x0F
// 0x3110     RFE_FW           AGCCFG0               [15]       MODE                  0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x3110     RFE_FW           AGCCFG0               [14]       DUAL                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3110     RFE_FW           AGCCFG0               [13]       RSSICHK               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3110     RFE_FW           AGCCFG0               [12]       PEAKEN                0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x3110     RFE_FW           AGCCFG0               [11:10]    MINSTEP               0x3            0x3           0x3              0x3               0x3               0x3           0x3           0x3
// 0x3110     RFE_FW           AGCCFG0               [9:5]      MAXGAIN               0x15           0x15          0x15             0x15              0x15              0x15          0x15          0x15
// 0x3110     RFE_FW           AGCCFG0               [4:0]      MINGAIN               0x03           0x03          0x03             0x03              0x03              0x03          0x03          0x03
// 0x3118     RFE_FW           AGCCFG2               [15:12]    POSEDGESTEP           0x6            0x6           0x6              0x6               0x6               0x6           0x6           0x6
// 0x3118     RFE_FW           AGCCFG2               [11:8]     SETTLEWAIT            0x4            0x5           0x4              0x5               0x5               0x5           0x5           0x5
// 0x3118     RFE_FW           AGCCFG2               [7]        INFAGCREFEN           0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3118     RFE_FW           AGCCFG2               [6]        SLOPEEN               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3118     RFE_FW           AGCCFG2               [4:0]      FILTDLY               0x03           0x03          0x03             0x03              0x04              0x03          0x03          0x03
// 0x311C     RFE_FW           AGCCFG3               [15:12]    NOMEASURE             0x2            0x2           0x2              0x2               0x2               0x2           0x2           0x2
// 0x311C     RFE_FW           AGCCFG3               [11:0]     TIMEOUT               0x06A          0x06A         0x06A            0x06A             0x06A             0x06A         0x06A         0x06A
// 0x317C     LRFDRFE          IQIC                  [8:7]      IMTHR                 0x1            0x0           0x1              0x1               0x1               0x1           0x1           0x1
// 0x317C     LRFDRFE          IQIC                  [6:5]      BLSETTLE              0x0            0x2           0x0              0x0               0x0               0x0           0x0           0x0
// 0x317C     LRFDRFE          IQIC                  [4:3]      BL                    0x2            0x3           0x2              0x2               0x2               0x2           0x2           0x2
// 0x317C     LRFDRFE          IQIC                  [2]        GEARING               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x317C     LRFDRFE          IQIC                  [1]        AUTOCOEFFCLR          0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x319C     LRFDRFE          RFECTL0               [15:8]     SPARE                 0x02           0x02          0x02             0x02              0x02              0x02          0x02          0x02
// 0x319C     LRFDRFE          RFECTL0               [7]        LDOREFBYP             0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x319C     LRFDRFE          RFECTL0               [6]        VREFBP                0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x319C     LRFDRFE          RFECTL0               [5]        TXWAITMOD             0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x319C     LRFDRFE          RFECTL0               [4]        PHEDISC               0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x319C     LRFDRFE          RFECTL0               [3]        RTRIMTST              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x319C     LRFDRFE          RFECTL0               [2]        IIR                   0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x319C     LRFDRFE          RFECTL0               [1]        REFDTHR               0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x319C     LRFDRFE          RFECTL0               [0]        FCDEM                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31A0     LRFDRFE          RFECTL1               [15:5]     SPARE                 0x004          0x004         0x004            0x004             -                 -             -             -
// 0x31A0     LRFDRFE          RFECTL1               [4]        IQIC                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31A0     LRFDRFE          RFECTL1               [3]        TXPA                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31A0     LRFDRFE          RFECTL1               [2]        TXOFDM                0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31A0     LRFDRFE          RFECTL1               [1]        RXCFG                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31A0     LRFDRFE          RFECTL1               [0]        ULNA                  0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x31BC     LRFDRFE          LNA                   [15:14]    RXTX                  0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x31BC     LRFDRFE          LNA                   [11:8]     MIXCAP                0xC            0xC           0xC              0x8               0x7               0x7           0x5           0x5
// 0x31BC     LRFDRFE          LNA                   [7:4]      TRIM                  <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x31BC     LRFDRFE          LNA                   [3]        BIAS                  0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x31BC     LRFDRFE          LNA                   [2:1]      IB                    0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x31C0     LRFDRFE          IFAMP                 [15]       IFFBYP                0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31C0     LRFDRFE          IFAMP                 [14:12]    IFFDC                 <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x31C0     LRFDRFE          IFAMP                 [11:8]     IFFRES                <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x31C0     LRFDRFE          IFAMP                 [7:4]      IFFCAP                0xC            0xC           0xC              0x8               0x7               0x7           0x5           0x5
// 0x31C0     LRFDRFE          IFAMP                 [3:1]      IB                    0x3            0x3           0x3              0x3               0x3               0x3           0x3           0x3
// 0x31C8     LRFDRFE          PA                    [12:8]     TRIM                  <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x31C8     LRFDRFE          PA                    [7]        BIAS                  0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x31C8     LRFDRFE          PA                    [6]        SHORT                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31C8     LRFDRFE          PA                    [5:4]      RC                    0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31C8     LRFDRFE          PA                    [3:2]      AD                    0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x31CC     LRFDRFE          ULNA                  [14]       TEMP                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31CC     LRFDRFE          ULNA                  [13]       LDOITST               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31CC     LRFDRFE          ULNA                  [12]       LDOATST               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31CC     LRFDRFE          ULNA                  [11]       MIXATST               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31CC     LRFDRFE          ULNA                  [10]       ITST                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31CC     LRFDRFE          ULNA                  [9:8]      FBRES                 0x2            0x2           0x2              0x2               0x2               0x2           0x2           0x2
// 0x31CC     LRFDRFE          ULNA                  [7:4]      TRIM                  <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x31CC     LRFDRFE          ULNA                  [3:1]      IB                    0x3            0x3           0x3              0x3               0x3               0x3           0x3           0x3
// 0x31D0     LRFDRFE          RFLDO                 [10:8]     IFAMPATTN             0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31D0     LRFDRFE          RFLDO                 [7:1]      TRIM                  <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x31D4     LRFDRFE          IFADC0                [14:12]    DITHERTRIM            0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31D4     LRFDRFE          IFADC0                [11:10]    DITHEREN              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31D4     LRFDRFE          IFADC0                [7:4]      INT2ADJ               <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x31D4     LRFDRFE          IFADC0                [1]        DTCICG                0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31D8     LRFDRFE          IFADC1                [15]       NRZ                   <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x31D8     LRFDRFE          IFADC1                [14:9]     TRIM                  <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x31D8     LRFDRFE          IFADC1                [4]        ADCLFSROUT            0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31D8     LRFDRFE          IFADC1                [3:1]      LPFTSTMODE            0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31D8     LRFDRFE          IFADC1                [0]        INVCLKOUT             0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31DC     LRFDRFE          IFADCLF               [15:12]    FF3                   <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x31DC     LRFDRFE          IFADCLF               [11:8]     FF2                   <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x31DC     LRFDRFE          IFADCLF               [7:4]      FF1                   <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x31DC     LRFDRFE          IFADCLF               [3:0]      INT3                  <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x31E0     LRFDRFE          IFADCQUANT            [15:14]    INT1ADJ               0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x31E0     LRFDRFE          IFADCQUANT            [13:9]     DBGCALVALIN           0x00           0x00          0x00             0x00              0x00              0x00          0x00          0x00
// 0x31E0     LRFDRFE          IFADCQUANT            [8]        DBGCALLEG             0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31E0     LRFDRFE          IFADCQUANT            [7:6]      DBGCALMQ              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31E0     LRFDRFE          IFADCQUANT            [5:4]      DBGCALMI              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31E0     LRFDRFE          IFADCQUANT            [2:0]      QUANTTHR              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x31E4     LRFDRFE          IFADCALDO             [15]       ATESTVSSANA           0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31E4     LRFDRFE          IFADCALDO             [13:8]     TRIMOUT               <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x31E4     LRFDRFE          IFADCALDO             [7]        PASSCUR               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31E4     LRFDRFE          IFADCALDO             [6]        ATESTOUT              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31E4     LRFDRFE          IFADCALDO             [5]        ATESTINPUTREF         0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31E4     LRFDRFE          IFADCALDO             [4]        ATESTERRAMP           0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31E4     LRFDRFE          IFADCALDO             [3]        ITEST                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31E4     LRFDRFE          IFADCALDO             [2]        BYPASS                0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31E4     LRFDRFE          IFADCALDO             [1]        CLAMP                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31EC     LRFDRFE          IFADC2                [13]       INT3MODE              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x31EC     LRFDRFE          IFADC2                [12]       INT2MODE              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x31EC     LRFDRFE          IFADC2                [6]        QCALDBIQ              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31EC     LRFDRFE          IFADC2                [5]        QCALDBC               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31EC     LRFDRFE          IFADC2                [4:0]      SEL                   0x00           0x00          0x00             0x00              0x00              0x00          0x00          0x00
// 0x31F4     LRFDRFE          ATSTREF0              [15:0]     MUX                   0x0000         0x0000        0x0000           0x0000            0x0000            0x0000        0x0000        0x0000
// 0x31F8     LRFDRFE          ATSTREF1              [15]       VREFBPDIS             0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31F8     LRFDRFE          ATSTREF1              [14:10]    IREFTRIM              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x31F8     LRFDRFE          ATSTREF1              [8]        OUTPUT2               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31F8     LRFDRFE          ATSTREF1              [7]        OUTPUT1               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x31FC     LRFDRFE          ATSTREF2              [15:0]     MUX                   0x0000         0x0000        0x0000           0x0000            0x0000            0x0000        0x0000        0x0000
// 0x3200     LRFDRFE          DIVCTL                [11]       TXBBOOST              0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x3200     LRFDRFE          DIVCTL                [10]       IFADCCLKFREQ          0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3204     LRFDRFE          DIV                   [15]       PDET                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3204     LRFDRFE          DIV                   [14:12]    NMIREFTRIM            0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3204     LRFDRFE          DIV                   [11:9]     PMIREFTRIM            0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3204     LRFDRFE          DIV                   [8:0]      RATIO                 0x104          0x104         0x104            0x104             0x104             0x104         0x104         0x104
// 0x3208     LRFDRFE          DIVLDO                [15]       ITEST                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3208     LRFDRFE          DIVLDO                [14:8]     VOUTTRIM              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x3208     LRFDRFE          DIVLDO                [7]        FASTLOOP              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3208     LRFDRFE          DIVLDO                [6:4]      TMUX                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3208     LRFDRFE          DIVLDO                [3]        PDSEL                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3208     LRFDRFE          DIVLDO                [2]        MODE                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3208     LRFDRFE          DIVLDO                [1]        BYPASS                0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x320C     LRFDRFE          TDCLDO                [15]       ITEST                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x320C     LRFDRFE          TDCLDO                [14:8]     VOUTTRIM              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x320C     LRFDRFE          TDCLDO                [7]        FASTLOOP              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x320C     LRFDRFE          TDCLDO                [6:4]      TMUX                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x320C     LRFDRFE          TDCLDO                [3]        PDSEL                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x320C     LRFDRFE          TDCLDO                [2]        MODE                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x320C     LRFDRFE          TDCLDO                [1]        BYPASS                0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3210     LRFDRFE          DCOLDO0               [15:8]     VOUTTRIM              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x3210     LRFDRFE          DCOLDO0               [7]        COMPCAP               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3210     LRFDRFE          DCOLDO0               [6]        CURRENT               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3210     LRFDRFE          DCOLDO0               [5]        ITEST                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3210     LRFDRFE          DCOLDO0               [4]        VTEST                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3210     LRFDRFE          DCOLDO0               [3]        LDOBYP                0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3210     LRFDRFE          DCOLDO0               [2]        LDOREFBYP             0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3210     LRFDRFE          DCOLDO0               [1]        SUPPLY                0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x3214     LRFDRFE          DCOLDO1               [11]       PEAKDETATST           0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3214     LRFDRFE          DCOLDO1               [10]       REFSRC                0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3214     LRFDRFE          DCOLDO1               [9:8]      DIVATST               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3218     LRFDRFE          DCO                   [8]        LATCHRSTEN            0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3218     LRFDRFE          DCO                   [6:3]      TAILRESTRIM           <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x3218     LRFDRFE          DCO                   [1]        HFOVR                 0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x3218     LRFDRFE          DCO                   [0]        CRSCAPCM              0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x321C     LRFDRFE          DIVPDET               [15]       RBIAS2                0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x321C     LRFDRFE          DIVPDET               [14:12]    JCNTFREQ              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x321C     LRFDRFE          DIVPDET               [11:9]     JCNTPH                0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x321C     LRFDRFE          DIVPDET               [8:7]      RBIAS10               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x321C     LRFDRFE          DIVPDET               [6]        PHOVR                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x321C     LRFDRFE          DIVPDET               [5:3]      REF                   <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x3220     LRFDRFE          TXOFDM0               [15]       IREF2U                0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3220     LRFDRFE          TXOFDM0               [14]       IREF20U               0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x3224     LRFDRFE          TXOFDM1               [15:12]    CMFBIREF              0x5            0x5           0x5              0x5               0x5               0x5           0x5           0x5
// 0x3224     LRFDRFE          TXOFDM1               [11:8]     PABIASIREF            0x5            0x5           0x5              0x5               0x5               0x5           0x5           0x5
// 0x3224     LRFDRFE          TXOFDM1               [7:4]      FILTIREF              0x5            0x5           0x5              0x5               0x5               0x5           0x5           0x5
// 0x3228     LRFDRFE          TXOFDM2               [15:12]    PPA2IREF              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x3228     LRFDRFE          TXOFDM2               [11:8]     PPA1IREF              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x3228     LRFDRFE          TXOFDM2               [7:4]      PAFIXBIASIREF         <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x3228     LRFDRFE          TXOFDM2               [3:0]      PPAFSKIREF            0x5            0x5           0x5              0x5               0x5               0x5           0x5           0x5
// 0x322C     LRFDRFE          TXOFDM3               [15]       ITESTBIASSEL          0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x322C     LRFDRFE          TXOFDM3               [14]       PPA2BIASSEL           0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x322C     LRFDRFE          TXOFDM3               [13]       PPA1BIASSEL           0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x322C     LRFDRFE          TXOFDM3               [12]       PAFIXBIASSEL          0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x322C     LRFDRFE          TXOFDM3               [11]       ITRIMSWEN             0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x322C     LRFDRFE          TXOFDM3               [10]       RESSWEN               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x322C     LRFDRFE          TXOFDM3               [9]        GNDSWEN               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x322C     LRFDRFE          TXOFDM3               [4]        DACLSB                0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x322C     LRFDRFE          TXOFDM3               [3]        DACCLKINV             0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3230     LRFDRFE          TXOFDM4               [10:9]     FILTIOUT              0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x3230     LRFDRFE          TXOFDM4               [8:6]      FILTPH90ITRIM         0x5            0x5           0x5              0x5               0x5               0x5           0x5           0x5
// 0x3230     LRFDRFE          TXOFDM4               [5:3]      FILTPH0ITRIM          0x5            0x5           0x5              0x5               0x5               0x5           0x5           0x5
// 0x3230     LRFDRFE          TXOFDM4               [2:0]      FILTVCMTRIM           <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x3234     LRFDRFE          TXOFDM5               [14:9]     FILTFBRTRIM           <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x3234     LRFDRFE          TXOFDM5               [8:3]      FILTINRTRIM           <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x3234     LRFDRFE          TXOFDM5               [2:0]      FILTRTRIM             <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x3238     LRFDRFE          TXOFDM6               [14:12]    BUFPH90ITRIM          0x4            0x4           0x4              0x4               0x4               0x4           0x4           0x4
// 0x3238     LRFDRFE          TXOFDM6               [11:9]     BUFPH0ITRIM           0x4            0x4           0x4              0x4               0x4               0x4           0x4           0x4
// 0x3238     LRFDRFE          TXOFDM6               [8:6]      FILTPH90FBCAP         0x3            0x3           0x3              0x3               0x3               0x3           0x3           0x3
// 0x3238     LRFDRFE          TXOFDM6               [5:3]      FILTPH0FBCAP          0x3            0x3           0x3              0x3               0x3               0x3           0x3           0x3
// 0x323C     LRFDRFE          TXOFDM7               [15:14]    CMFBGAIN              0x2            0x2           0x2              0x2               0x2               0x2           0x2           0x2
// 0x323C     LRFDRFE          TXOFDM7               [13]       PPA1HP                0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x323C     LRFDRFE          TXOFDM7               [12:11]    PPAFSKATTN            0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x323C     LRFDRFE          TXOFDM7               [10:8]     PPA1RTRIM             0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x323C     LRFDRFE          TXOFDM7               [7:6]      PPA1PH90GAIN          0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x323C     LRFDRFE          TXOFDM7               [5:4]      PPA1PH0GAIN           0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x323C     LRFDRFE          TXOFDM7               [3:0]      MIXDCBIAS             0x4            0x4           0x4              0x4               0x4               0x4           0x4           0x4
// 0x3240     LRFDRFE          TXOFDM8               [6]        PPA2HP                0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3240     LRFDRFE          TXOFDM8               [5:2]      PPA2RTRIM             0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3240     LRFDRFE          TXOFDM8               [1:0]      PPA2GAIN              0x2            0x2           0x2              0x2               0x2               0x2           0x2           0x2
// 0x3244     LRFDRFE          TXOFDM9               [13]       ENVDREF               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3244     LRFDRFE          TXOFDM9               [12:9]     ENVDPBLDR             0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3244     LRFDRFE          TXOFDM9               [8:5]      ENVDNBLDR             0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3244     LRFDRFE          TXOFDM9               [4:0]      ENVDNBIAS             0x10           0x10          0x10             0x10              0x10              0x10          0x10          0x10
// 0x3248     LRFDRFE          TXOFDM10              [15]       PAFIXBIAS             0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x3248     LRFDRFE          TXOFDM10              [14]       ENVDBIAS              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3248     LRFDRFE          TXOFDM10              [13:8]     PAFIXBIASVREF         0x26           0x26          0x26             0x26              0x26              0x26          0x26          0x26
// 0x3248     LRFDRFE          TXOFDM10              [7:4]      ENVDVREF              0x6            0x6           0x6              0x6               0x6               0x6           0x6           0x6
// 0x3248     LRFDRFE          TXOFDM10              [3:0]      ENVDIREF              0x6            0x6           0x6              0x6               0x6               0x6           0x6           0x6
// 0x324C     LRFDRFE          TXOFDM11              [14:13]    PPA2RBIASMSB          0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x324C     LRFDRFE          TXOFDM11              [12:11]    PPA2RBIASLSB          0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x324C     LRFDRFE          TXOFDM11              [6:4]      PACASLOW              0x2            0x2           0x2              0x2               0x2               0x2           0x2           0x2
// 0x324C     LRFDRFE          TXOFDM11              [3]        PACASSHRT             0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x324C     LRFDRFE          TXOFDM11              [2:0]      PACASHGH              0x2            0x2           0x2              0x2               0x2               0x2           0x2           0x2
// 0x3250     LRFDRFE          TXOFDM12              [15]       PATEMP                0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3250     LRFDRFE          TXOFDM12              [8:0]      TESTCTL               0x000          0x000         0x000            0x000             0x000             0x000         0x000         0x000
// 0x3254     LRFDRFE          TXOFDM13              [15:14]    SPARE14               0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x3254     LRFDRFE          TXOFDM13              [13:11]    PAPDETIREF            0x2            0x2           0x2              0x2               0x2               0x2           0x2           0x2
// 0x3254     LRFDRFE          TXOFDM13              [10:9]     PAPDETGAIN            0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3254     LRFDRFE          TXOFDM13              [8]        PAPDET                0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3254     LRFDRFE          TXOFDM13              [7:6]      SPARE6                0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x3254     LRFDRFE          TXOFDM13              [5:3]      PPA2PDETRTRIM         <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x3254     LRFDRFE          TXOFDM13              [2:1]      PPA2PDETIREF          0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x3254     LRFDRFE          TXOFDM13              [0]        PPA2PDET              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3258     LRFDRFE          TXOFDM14              [15:14]    FSKRAMPCAP            0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3258     LRFDRFE          TXOFDM14              [13:11]    FSKRAMPIREF           0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3258     LRFDRFE          TXOFDM14              [10:9]     FSKRAMPIDIS           0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3258     LRFDRFE          TXOFDM14              [8]        FSKRAMPDNCTL          0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3258     LRFDRFE          TXOFDM14              [7:6]      FSKRAMPITAIL          0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3258     LRFDRFE          TXOFDM14              [5]        BIASGENCON            0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x3258     LRFDRFE          TXOFDM14              [4]        FSKRAMPOUT            0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3258     LRFDRFE          TXOFDM14              [3]        FSKRAMPBYP            0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x3258     LRFDRFE          TXOFDM14              [2]        FSKRAMPOUTPD          0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3258     LRFDRFE          TXOFDM14              [1]        FSKRAMPIN             0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3258     LRFDRFE          TXOFDM14              [0]        FSKRAMPCTL            0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3260     LRFDRFE          TXOFDMLDO             [15]       ITEST                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3260     LRFDRFE          TXOFDMLDO             [14:8]     VOUTTRIM              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>        <TRIM>        <TRIM>
// 0x3260     LRFDRFE          TXOFDMLDO             [7]        FASTLOOP              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3260     LRFDRFE          TXOFDMLDO             [6:4]      TMUX                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3260     LRFDRFE          TXOFDMLDO             [3]        PDSEL                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3260     LRFDRFE          TXOFDMLDO             [2]        MODE                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3260     LRFDRFE          TXOFDMLDO             [1]        BYPASS                0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3268     LRFDRFE          PRE0                  [13:8]     PLLDIV1               0x08           0x08          0x08             0x08              0x08              0x08          0x08          0x08
// 0x3268     LRFDRFE          PRE0                  [5:0]      PLLDIV0               0x07           0x07          0x07             0x07              0x07              0x07          0x07          0x07
// 0x326C     LRFDRFE          PRE1                  [5:0]      HSDDC                 0x00           0x00          0x00             0x00              0x00              0x00          0x00          0x00
// 0x3270     LRFDRFE          PRE2                  [15:12]    MIDCALDIVLSB          0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3270     LRFDRFE          PRE2                  [11:6]     CRSCALDIV             0x18           0x18          0x18             0x18              0x18              0x18          0x18          0x18
// 0x3270     LRFDRFE          PRE2                  [5:0]      FSMDIV                0x05           0x05          0x05             0x05              0x05              0x05          0x05          0x05
// 0x3274     LRFDRFE          PRE3                  [15:5]     FINECALDIV            0x030          0x030         0x030            0x030             0x030             0x030         0x030         0x030
// 0x3274     LRFDRFE          PRE3                  [4:0]      MIDCALDIVMSB          0x03           0x03          0x03             0x03              0x03              0x03          0x03          0x03
// 0x3278     LRFDRFE          CAL0                  [14:8]     FCSTART               0x40           0x40          0x40             0x40              0x40              0x40          0x40          0x40
// 0x3278     LRFDRFE          CAL0                  [3:2]      TDCAVG                0x2            0x2           0x2              0x2               0x2               0x2           0x2           0x2
// 0x327C     LRFDRFE          CAL1                  [14:8]     FCTOP                 0x7F           0x7F          0x7F             0x7F              0x7F              0x7F          0x7F          0x7F
// 0x327C     LRFDRFE          CAL1                  [6:0]      FCBOT                 0x00           0x00          0x00             0x00              0x00              0x00          0x00          0x00
// 0x3288     LRFDRFE          MISC0                 [15]       TDCCALENHCTL          0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x3288     LRFDRFE          MISC0                 [14]       TDCCALENHCFG          0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x3288     LRFDRFE          MISC0                 [13]       PHCPT                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3288     LRFDRFE          MISC0                 [12]       TDCCALCORR            0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x3288     LRFDRFE          MISC0                 [11]       TDCMSBCORR            0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x3288     LRFDRFE          MISC0                 [5:4]      DLYCANCRS             0x2            0x2           0x2              0x2               0x2               0x2           0x2           0x2
// 0x3298     LRFDRFE          PHEDISC               [13:10]    CNT                   0x3            0x3           0x3              0x3               0x3               0x3           0x3           0x3
// 0x3298     LRFDRFE          PHEDISC               [9:0]      THR                   0x007          0x007         0x007            0x007             0x007             0x007         0x007         0x007
// 0x32A0     LRFDRFE          PLLMON0               [15:14]    PHELOLCNT             0x3            0x3           0x3              0x3               0x3               0x3           0x3           0x3
// 0x32A0     LRFDRFE          PLLMON0               [13:8]     PHELOLTHR             0x1F           0x1F          0x1F             0x1F              0x1F              0x1F          0x1F          0x1F
// 0x32A0     LRFDRFE          PLLMON0               [6:0]      FCTHR                 0x7F           0x7F          0x7F             0x7F              0x7F              0x7F          0x7F          0x7F
// 0x32A4     LRFDRFE          PLLMON1               [12:8]     PHELOCKCNT            0x04           0x04          0x04             0x04              0x04              0x04          0x04          0x04
// 0x32A4     LRFDRFE          PLLMON1               [7:0]      PHELOCKTHR            0x10           0x10          0x10             0x10              0x10              0x10          0x10          0x10
// 0x3370     LRFDRFE          DCOHFT0               [15:12]    C3                    0x3            0x3           0x3              0x3               0x3               0x3           0x3           0x3
// 0x3370     LRFDRFE          DCOHFT0               [11:8]     C2                    0x2            0x2           0x2              0x2               0x2               0x2           0x2           0x2
// 0x3370     LRFDRFE          DCOHFT0               [7:4]      C1                    0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x3370     LRFDRFE          DCOHFT0               [3:0]      C0                    0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3374     LRFDRFE          DCOHFT1               [15:12]    C7                    0x7            0x7           0x7              0x7               0x7               0x7           0x7           0x7
// 0x3374     LRFDRFE          DCOHFT1               [11:8]     C6                    0x6            0x6           0x6              0x6               0x6               0x6           0x6           0x6
// 0x3374     LRFDRFE          DCOHFT1               [7:4]      C5                    0x5            0x5           0x5              0x5               0x5               0x5           0x5           0x5
// 0x3374     LRFDRFE          DCOHFT1               [3:0]      C4                    0x4            0x4           0x4              0x4               0x4               0x4           0x4           0x4
// 0x3378     LRFDRFE          DCOHFT2               [15:12]    C11                   0xB            0xB           0xB              0xB               0xB               0xB           0xB           0xB
// 0x3378     LRFDRFE          DCOHFT2               [11:8]     C10                   0xA            0xA           0xA              0xA               0xA               0xA           0xA           0xA
// 0x3378     LRFDRFE          DCOHFT2               [7:4]      C9                    0x9            0x9           0x9              0x9               0x9               0x9           0x9           0x9
// 0x3378     LRFDRFE          DCOHFT2               [3:0]      C8                    0x8            0x8           0x8              0x8               0x8               0x8           0x8           0x8
// 0x337C     LRFDRFE          DCOHFT3               [15:12]    C15                   0xF            0xF           0xF              0xF               0xF               0xF           0xF           0xF
// 0x337C     LRFDRFE          DCOHFT3               [11:8]     C14                   0xE            0xE           0xE              0xE               0xE               0xE           0xE           0xE
// 0x337C     LRFDRFE          DCOHFT3               [7:4]      C13                   0xD            0xD           0xD              0xD               0xD               0xD           0xD           0xD
// 0x337C     LRFDRFE          DCOHFT3               [3:0]      C12                   0xC            0xC           0xC              0xC               0xC               0xC           0xC           0xC
// 0x3380     LRFDRFE          DCOHFB0               [15:12]    C3                    0x3            0x3           0x3              0x3               0x3               0x3           0x3           0x3
// 0x3380     LRFDRFE          DCOHFB0               [11:8]     C2                    0x2            0x2           0x2              0x2               0x2               0x2           0x2           0x2
// 0x3380     LRFDRFE          DCOHFB0               [7:4]      C1                    0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x3380     LRFDRFE          DCOHFB0               [3:0]      C0                    0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x3384     LRFDRFE          DCOHFB1               [15:12]    C7                    0x7            0x7           0x7              0x7               0x7               0x7           0x7           0x7
// 0x3384     LRFDRFE          DCOHFB1               [11:8]     C6                    0x6            0x6           0x6              0x6               0x6               0x6           0x6           0x6
// 0x3384     LRFDRFE          DCOHFB1               [7:4]      C5                    0x5            0x5           0x5              0x5               0x5               0x5           0x5           0x5
// 0x3384     LRFDRFE          DCOHFB1               [3:0]      C4                    0x4            0x4           0x4              0x4               0x4               0x4           0x4           0x4
// 0x3388     LRFDRFE          DCOHFB2               [15:12]    C11                   0xB            0xB           0xB              0xB               0xB               0xB           0xB           0xB
// 0x3388     LRFDRFE          DCOHFB2               [11:8]     C10                   0xA            0xA           0xA              0xA               0xA               0xA           0xA           0xA
// 0x3388     LRFDRFE          DCOHFB2               [7:4]      C9                    0x9            0x9           0x9              0x9               0x9               0x9           0x9           0x9
// 0x3388     LRFDRFE          DCOHFB2               [3:0]      C8                    0x8            0x8           0x8              0x8               0x8               0x8           0x8           0x8
// 0x338C     LRFDRFE          DCOHFB3               [15:12]    C15                   0xF            0xF           0xF              0xF               0xF               0xF           0xF           0xF
// 0x338C     LRFDRFE          DCOHFB3               [11:8]     C14                   0xE            0xE           0xE              0xE               0xE               0xE           0xE           0xE
// 0x338C     LRFDRFE          DCOHFB3               [7:4]      C13                   0xD            0xD           0xD              0xD               0xD               0xD           0xD           0xD
// 0x338C     LRFDRFE          DCOHFB3               [3:0]      C12                   0xC            0xC           0xC              0xC               0xC               0xC           0xC           0xC
// 0x3390     LRFDRFE          SPARE0                [15:0]     VAL                   0x5090         0x5090        0x5090           0x5090            0x5090            0x5090        0x5090        0x5090
// 0x3394     RFE_FW           PHYRX0RSSI            [7:0]      OFFSET                0x47           0x47          0x47             0x47              0x47              0x47          0x47          0x47
// 0x3398     RFE_FW           PHYRX1RSSI            [7:0]      OFFSET                0x47           0x47          0x47             0x47              0x47              0x47          0x47          0x47
// 0x339C     RFE_FW           PHYAGCCFG4            [15:8]     NOSIGNALTHR           0x05           0x05          0x05             0x05              0x05              0x05          0x05          0x05
// 0x339C     RFE_FW           PHYAGCCFG4            [7:0]      AGCIRQTHR             0x27           0x27          0x28             0x28              0x27              0x29          0x29          0x2A
// 0x33A0     RFE_FW           RXCODCCFG2            [7:5]      IIRGAINUSLOW          0x4            0x4           0x4              0x4               0x4               0x4           0x4           0x4
// 0x33A0     RFE_FW           RXCODCCFG2            [4]        IIRENUSLOW            0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x33A0     RFE_FW           RXCODCCFG2            [3]        ACCMODEUSLOW          0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x33A0     RFE_FW           RXCODCCFG2            [2:1]      ACCPERUSLOW           0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x33A0     RFE_FW           RXCODCCFG2            [0]        ACCENUSLOW            0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x33A4     RFE_FW           RXFRONTEND            [13:12]    PINSEL                0x2            0x2           0x2              0x2               0x2               0x2           0x2           0x2
// 0x33A8     RFE_FW           AGCCFG5               [7:0]      DELTAGCREF            0x01           0x01          0x01             0x01              0x01              0x01          0x01          0x02
// 0x33AC     RFE_FW           IFADCPEAKTHR          [7:0]      THR                   0x08           0x08          0x08             0x08              0x08              0x08          0x08          0x08
// 0x33B8     RFE_FW           FEDELTARSSI           [15:0]     OFFSET                0x0000         0x0000        0x0000           0x0000            0x0000            0x0000        0x0000        0x0000
// 0x33BC     RFE_FW           FEDELTANOISEFIGURE    [15:0]     VAL                   0x0000         0x0000        0x0000           0x0000            0x0000            0x0000        0x0000        0x0000
// 0x33C0     RFE_FW           AGCCFG6               [15:0]     SLOPETHR              0x0000         0x0000        0x0000           0x0000            0x0000            0x0000        0x0000        0x0000
// 0x33C8     RFE_FW           AGCCFG8               [15:0]     STOPCLIMBTHR          0x0055         0x0055        0x0055           0x0055            0x0055            0x0055        0x0055        0x0055
// 0x0018     PBE_RAM          CSCTL2DEFAULT         [15:8]     RSSITHRESH            0xA1           0xA1          0xA1             0xA1              0xA1              0xA1          0xA1          0xA1
// 0x0018     PBE_RAM          CSCTL2DEFAULT         [7:4]      NUMRSSIHITSBUSY       0x4            0x4           0x4              0x4               0x4               0x4           0x4           0x4
// 0x0018     PBE_RAM          CSCTL2DEFAULT         [3:0]      RSSIWINDOW            0x8            0x8           0x8              0x8               0x8               0x8           0x8           0x8
// 0x001A     PBE_RAM          CSCTL3DEFAULT         [15:8]     PDPERIOD              0x05           0x05          0x05             0x05              0x05              0x05          0x05          0x05
// 0x001A     PBE_RAM          CSCTL3DEFAULT         [7:4]      NUMPDHITSBUSY         0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x001A     PBE_RAM          CSCTL3DEFAULT         [3:0]      PDWINDOW              0x8            0x8           0x8              0x8               0x8               0x8           0x8           0x8
// 0x6804     RFE_RAM          TDCCAL0               [10:8]     STOP                  0x3            0x3           0x3              0x3               0x3               0x3           0x3           0x3
// 0x6806     RFE_RAM          TDCCAL1               [7:0]      SUB                   0x00           0x00          0x00             0x00              0x00              0x00          0x00          0x00
// 0x6808     RFE_RAM          TDCCAL2               [5:0]      AVG                   0x10           0x10          0x10             0x10              0x10              0x10          0x10          0x10
// 0x680A     RFE_RAM          TDCPLL                [10:8]     STOP                  0x4            0x4           0x4              0x4               0x4               0x4           0x4           0x4
// 0x680C     RFE_RAM          K1LSB                 [15:0]     VAL                   0x569B         0x569B        0x569B           0x569B            0x569B            0x569B        0x569B        0x569B
// 0x680E     RFE_RAM          K1MSB                 [15:0]     VAL                   0x010A         0x010A        0x010A           0x010A            0x010A            0x010A        0x010A        0x010A
// 0x6810     RFE_RAM          K2BL                  [15]       HPM                   0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x6810     RFE_RAM          K2BL                  [14:0]     VAL                   0x0383         0x0383        0x0383           0x0383            0x0383            0x0383        0x0383        0x0383
// 0x6812     RFE_RAM          K2AL                  [15]       HPM                   0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x6812     RFE_RAM          K2AL                  [14:0]     VAL                   0x056B         0x056B        0x056B           0x056B            0x056B            0x056B        0x056B        0x056B
// 0x6814     RFE_RAM          K3BL                  [15:0]     VAL                   0x2659         0x2659        0x2659           0x2659            0x2659            0x2659        0x2659        0x2659
// 0x6816     RFE_RAM          K3AL                  [15:0]     VAL                   0x02E0         0x02E0        0x02E0           0x02E0            0x02E0            0x02E0        0x02E0        0x02E0
// 0x681E     RFE_RAM          RTRIMOFF              [3:0]      VAL                   0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x6820     RFE_RAM          RTRIMMIN              [3:0]      VAL                   0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x6822     RFE_RAM          DIVI                  [15]       PDET                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x6822     RFE_RAM          DIVI                  [14:12]    NMIREFTRIM            0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x6822     RFE_RAM          DIVI                  [11:9]     PMIREFTRIM            0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x6822     RFE_RAM          DIVI                  [8:0]      RATIO                 0x104          0x104         0x104            0x104             0x104             0x104         0x104         0x104
// 0x6824     RFE_RAM          DIVF                  [15]       PDET                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x6824     RFE_RAM          DIVF                  [14:12]    NMIREFTRIM            0x4            0x4           0x4              0x4               0x4               0x4           0x4           0x4
// 0x6824     RFE_RAM          DIVF                  [11:9]     PMIREFTRIM            0x4            0x4           0x4              0x4               0x4               0x4           0x4           0x4
// 0x6824     RFE_RAM          DIVF                  [8:0]      RATIO                 0x104          0x104         0x104            0x104             0x104             0x104         0x104         0x104
// 0x6826     RFE_RAM          DIVLDOI               [14:8]     VOUTTRIM              0x00           0x00          0x00             0x00              0x00              0x00          0x00          0x00
// 0x6828     RFE_RAM          DIVLDOF               [14:8]     VOUTTRIM              0x00           0x00          0x00             0x00              0x00              0x00          0x00          0x00
// 0x682A     RFE_RAM          DIVLDOIOFF            [6:0]      VAL                   0x14           0x14          0x14             0x14              0x14              0x14          0x14          0x14
// 0x682C     RFE_RAM          LDOSETTLE             [9:0]      VAL                   0x138          0x138         0x138            0x138             0x138             0x138         0x138         0x138
// 0x682E     RFE_RAM          CHRGSETTLE            [9:0]      VAL                   0x000          0x000         0x000            0x000             0x000             0x000         0x000         0x000
// 0x6830     RFE_RAM          DIVSETTLE             [9:0]      VAL                   0x018          0x018         0x018            0x018             0x018             0x018         0x018         0x018
// 0x6832     RFE_RAM          RFLDOTX               [7:1]      TRIM                  0x00           0x00          0x00             0x00              0x00              0x00          0x00          0x00
// 0x6834     RFE_RAM          RFLDORX               [7:1]      TRIM                  0x00           0x00          0x00             0x00              0x00              0x00          0x00          0x00
// 0x6836     RFE_RAM          RFLDOOFDMTX           [7:1]      TRIM                  0x00           0x00          0x00             0x00              0x00              0x00          0x00          0x00
// 0x6838     RFE_RAM          RTRIMTX               [6:3]      TRIM                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x683A     RFE_RAM          RTRIMRX               [6:3]      TRIM                  0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x683C     RFE_RAM          DCOAMPSETTLE          [9:0]      VAL                   0x048          0x048         0x048            0x048             0x048             0x048         0x048         0x048
// 0x683E     RFE_RAM          DCOLDO0VTRIM          [7:0]      VOUTTRIM              0x00           0x00          0x00             0x00              0x00              0x00          0x00          0x00
// 0x6840     RFE_RAM          DCOLDO0VMAX           [7:0]      VOUTTRIM              0xFF           0xFF          0xFF             0xFF              0xFF              0xFF          0xFF          0xFF
// 0x6842     RFE_RAM          DCOOVR0INIT           [13:8]     MIDCODE               0x3F           0x3F          0x3F             0x3F              0x3F              0x3F          0x3F          0x3F
// 0x6842     RFE_RAM          DCOOVR0INIT           [7:4]      CRSCODE               0xF            0xF           0xF              0xF               0xF               0xF           0xF           0xF
// 0x6842     RFE_RAM          DCOOVR0INIT           [1]        MIDCTL                0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x6842     RFE_RAM          DCOOVR0INIT           [0]        CRSCTL                0x1            0x1           0x1              0x1               0x1               0x1           0x1           0x1
// 0x6844     RFE_RAM          CALTIMEOUT            [15:0]     VAL                   0x0FFF         0x0FFF        0x0FFF           0x0FFF            0x0FFF            0x0FFF        0x0FFF        0x0FFF
// 0x6846     RFE_RAM          FSTXTONE              [15:0]     VAL                   0x00F0         0x00F0        0x00F0           0x00F0            0x00F0            0x00F0        0x00F0        0x00F0
// 0x6848     RFE_RAM          FSBWSETTLE            [7:0]      VAL                   0x18           0x18          0x18             0x18              0x18              0x18          0x18          0x18
// 0x684A     RFE_RAM          IFADCDLDORX           [13:8]     TRIMOUT               0x00           0x00          0x00             0x00              0x00              0x00          0x00          0x00
// 0x684A     RFE_RAM          IFADCDLDORX           [6]        ATESTOUT              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x684A     RFE_RAM          IFADCDLDORX           [5]        ATESTFB               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x684A     RFE_RAM          IFADCDLDORX           [4]        ATESTERRAMP           0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x684A     RFE_RAM          IFADCDLDORX           [3]        ITEST                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x684A     RFE_RAM          IFADCDLDORX           [2]        BYPASS                0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x684A     RFE_RAM          IFADCDLDORX           [1]        CLAMP                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x684C     RFE_RAM          IFADCDLDOTX           [13:8]     TRIMOUT               0x00           0x00          0x00             0x00              0x00              0x00          0x00          0x00
// 0x684C     RFE_RAM          IFADCDLDOTX           [6]        ATESTOUT              0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x684C     RFE_RAM          IFADCDLDOTX           [5]        ATESTFB               0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x684C     RFE_RAM          IFADCDLDOTX           [4]        ATESTERRAMP           0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x684C     RFE_RAM          IFADCDLDOTX           [3]        ITEST                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x684C     RFE_RAM          IFADCDLDOTX           [2]        BYPASS                0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x684C     RFE_RAM          IFADCDLDOTX           [1]        CLAMP                 0x0            0x0           0x0              0x0               0x0               0x0           0x0           0x0
// 0x684E     RFE_RAM          DACTRIMADD            [5:0]      VAL                   0x14           0x14          0x14             0x14              0x14              0x14          0x14          0x14


// MAIN : OFDM OPTION register field values

// Address    Module           Register              Bit(s)     Field                 Value (1)      Value (2)     Value (3MCS0)    Value (3MCS16)    Value (4MCS01)    Value (4MCS26)
// ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// 0x115C     PBE_FW           PKTCFG                [12]       DATABITREV            0x0            0x0           0x0              0x0               0x0               0x0
// 0x115C     PBE_FW           PKTCFG                [11]       DATAWHBEFORECRC       0x0            0x0           0x0              0x0               0x0               0x0
// 0x115C     PBE_FW           PKTCFG                [10]       DATAWHEN              0x1            0x1           0x1              0x1               0x1               0x1
// 0x115C     PBE_FW           PKTCFG                [9]        DATACRCEN             0x1            0x1           0x1              0x1               0x1               0x1
// 0x115C     PBE_FW           PKTCFG                [4:3]      CRCSIZE               0x3            0x3           0x3              0x3               0x3               0x3
// 0x115C     PBE_FW           PKTCFG                [2]        WHCRCBITS             0x1            0x1           0x1              0x1               0x1               0x1
// 0x115C     PBE_FW           PKTCFG                [1]        WHITENER              0x1            0x1           0x1              0x1               0x1               0x1
// 0x115C     PBE_FW           PKTCFG                [0]        SUNMODE               0x1            0x1           0x1              0x1               0x1               0x1
// 0x1168     PBE_FW           OFDM0                 [9:8]      SUNOPTION             0x0            0x1           0x2              0x2               0x3               0x3
// 0x1168     PBE_FW           OFDM0                 [0]        SUNOFDM               0x1            0x1           0x1              0x1               0x1               0x1
// 0x3064     LRFDRFE          TXENABLE              [4]        INTP4                 0x0            0x0           0x0              0x0               0x1               0x1
// 0x3064     LRFDRFE          TXENABLE              [3]        INTP3                 0x0            0x0           0x1              0x1               0x1               0x1
// 0x3064     LRFDRFE          TXENABLE              [2]        INTP2                 0x0            0x1           0x1              0x1               0x1               0x1
// 0x3064     LRFDRFE          TXENABLE              [1]        INTP1                 0x1            0x1           0x1              0x1               0x1               0x1
// 0x3064     LRFDRFE          TXENABLE              [0]        FRONTEND              0x0            0x0           0x0              0x0               0x0               0x0
// 0x30A4     LRFDRFE          RX0CODC0              [11]       ESTSEL                0x1            0x1           0x1              0x1               0x1               0x1
// 0x30A4     LRFDRFE          RX0CODC0              [10:9]     COMPSEL               0x3            0x3           0x3              0x3               0x3               0x3
// 0x30A4     LRFDRFE          RX0CODC0              [8]        IIRUSEINITIAL         0x0            0x0           0x0              0x0               0x0               0x0
// 0x30B8     LRFDRFE          RX0BDEC               [7:6]      BDE2DVGA              0x0            0x0           0x0              0x0               0x0               0x0
// 0x30B8     LRFDRFE          RX0BDEC               [5:4]      BDE1DVGA              0x0            0x0           0x0              0x0               0x0               0x0
// 0x30B8     LRFDRFE          RX0BDEC               [3:2]      BDEC2NUMSTAGES        0x0            0x0           0x0              0x0               0x0               0x0
// 0x30B8     LRFDRFE          RX0BDEC               [1:0]      BDEC1NUMSTAGES        0x0            0x1           0x0              0x0               0x0               0x0
// 0x30C0     LRFDRFE          RX0CICHF              [7:6]      CICDVGA               0x0            0x0           0x1              0x1               0x1               0x1
// 0x30C0     LRFDRFE          RX0CICHF              [5:2]      CICDEC                0x1            0x1           0x5              0x5               0x9               0x9
// 0x30C0     LRFDRFE          RX0CICHF              [1:0]      CHFIBW                0x0            0x0           0x1              0x1               0x1               0x1
// 0x30C8     LRFDRFE          RX0MAGNCTRL0          [12]       PERMODE               0x1            0x1           0x1              0x1               0x1               0x1
// 0x30C8     LRFDRFE          RX0MAGNCTRL0          [11:8]     SCALE                 0x4            0x4           0x4              0x4               0x3               0x3
// 0x30C8     LRFDRFE          RX0MAGNCTRL0          [7:0]      PERIOD                0x0F           0x0F          0x0F             0x0F              0x07              0x07
// 0x30CC     LRFDRFE          RX0MAGNCTRL1          [12]       PERMODE               0x1            0x1           0x1              0x1               0x1               0x1
// 0x30CC     LRFDRFE          RX0MAGNCTRL1          [11:8]     SCALE                 0x8            0x8           0x8              0x8               0x6               0x6
// 0x30CC     LRFDRFE          RX0MAGNCTRL1          [7:0]      PERIOD                0xFF           0xFF          0xFF             0xFF              0x3F              0x3F
// 0x30D0     LRFDRFE          RX0MAGNCTRL2          [12]       PERMODE               0x1            0x1           0x1              0x1               0x1               0x1
// 0x30D0     LRFDRFE          RX0MAGNCTRL2          [11:8]     SCALE                 0x8            0x8           0x8              0x8               0x6               0x6
// 0x30D0     LRFDRFE          RX0MAGNCTRL2          [7:0]      PERIOD                0xFF           0xFF          0xFF             0xFF              0x3F              0x3F
// 0x30E4     LRFDRFE          RX1BDEC               [7:6]      BDE2DVGA              0x0            0x0           0x0              0x0               0x0               0x0
// 0x30E4     LRFDRFE          RX1BDEC               [3:2]      BDEC2NUMSTAGES        0x0            0x0           0x0              0x0               0x0               0x0
// 0x30EC     LRFDRFE          RX1CICHF              [11:10]    ACC1MGE2SRCSEL        0x2            0x2           0x2              0x2               0x2               0x2
// 0x30EC     LRFDRFE          RX1CICHF              [7:6]      CICDVGA               0x0            0x0           0x1              0x1               0x1               0x1
// 0x30EC     LRFDRFE          RX1CICHF              [5:2]      CICDEC                0x1            0x1           0x5              0x5               0x9               0x9
// 0x30EC     LRFDRFE          RX1CICHF              [1:0]      CHFIBW                0x0            0x0           0x1              0x1               0x1               0x1
// 0x30F4     LRFDRFE          RX1MAGNCTRL0          [12]       PERMODE               0x1            0x1           0x1              0x1               0x1               0x1
// 0x30F4     LRFDRFE          RX1MAGNCTRL0          [11:8]     SCALE                 0x4            0x4           0x4              0x4               0x3               0x3
// 0x30F4     LRFDRFE          RX1MAGNCTRL0          [7:0]      PERIOD                0x0F           0x0F          0x0F             0x0F              0x07              0x07
// 0x30F8     LRFDRFE          RX1MAGNCTRL1          [12]       PERMODE               0x1            0x1           0x1              0x1               0x1               0x1
// 0x30F8     LRFDRFE          RX1MAGNCTRL1          [11:8]     SCALE                 0x8            0x8           0x8              0x8               0x8               0x8
// 0x30F8     LRFDRFE          RX1MAGNCTRL1          [7:0]      PERIOD                0xFF           0xFF          0xFF             0xFF              0xFF              0xFF
// 0x30FC     LRFDRFE          RX1MAGNCTRL2          [12]       PERMODE               0x1            0x1           0x1              0x1               0x1               0x1
// 0x30FC     LRFDRFE          RX1MAGNCTRL2          [11:8]     SCALE                 0x4            0x4           0x4              0x4               0x8               0x8
// 0x30FC     LRFDRFE          RX1MAGNCTRL2          [7:0]      PERIOD                0x0F           0x0F          0x0F             0x0F              0xFF              0xFF
// 0x3110     RFE_FW           AGCCFG0               [15]       MODE                  0x1            0x1           0x1              0x1               0x1               0x1
// 0x3110     RFE_FW           AGCCFG0               [14]       DUAL                  0x0            0x0           0x0              0x0               0x0               0x0
// 0x3110     RFE_FW           AGCCFG0               [13]       RSSICHK               0x0            0x0           0x0              0x0               0x0               0x0
// 0x3110     RFE_FW           AGCCFG0               [12]       PEAKEN                0x0            0x0           0x0              0x0               0x0               0x0
// 0x3110     RFE_FW           AGCCFG0               [11:10]    MINSTEP               0x1            0x1           0x1              0x1               0x1               0x1
// 0x3110     RFE_FW           AGCCFG0               [9:5]      MAXGAIN               0x15           0x15          0x15             0x15              0x15              0x15
// 0x3110     RFE_FW           AGCCFG0               [4:0]      MINGAIN               0x03           0x03          0x03             0x03              0x03              0x03
// 0x3118     RFE_FW           AGCCFG2               [15:12]    POSEDGESTEP           0xA            0x9           0x9              0x9               0xA               0xA
// 0x3118     RFE_FW           AGCCFG2               [11:8]     SETTLEWAIT            0x8            0x9           0x8              0x8               0x8               0x8
// 0x3118     RFE_FW           AGCCFG2               [7]        INFAGCREFEN           0x1            0x1           0x1              0x1               0x1               0x1
// 0x3118     RFE_FW           AGCCFG2               [6]        SLOPEEN               0x1            0x1           0x1              0x1               0x1               0x1
// 0x3118     RFE_FW           AGCCFG2               [4:0]      FILTDLY               0x06           0x06          0x06             0x06              0x06              0x06
// 0x311C     RFE_FW           AGCCFG3               [15:12]    NOMEASURE             0x2            0x2           0x2              0x2               0x2               0x2
// 0x311C     RFE_FW           AGCCFG3               [11:0]     TIMEOUT               0x035          0x06A         0x06A            0x06A             0x13E             0x0D4
// 0x317C     LRFDRFE          IQIC                  [8:7]      IMTHR                 0x1            0x1           0x1              0x1               0x1               0x1
// 0x317C     LRFDRFE          IQIC                  [6:5]      BLSETTLE              0x0            0x0           0x0              0x0               0x0               0x0
// 0x317C     LRFDRFE          IQIC                  [4:3]      BL                    0x2            0x2           0x2              0x2               0x2               0x2
// 0x317C     LRFDRFE          IQIC                  [2]        GEARING               0x0            0x0           0x0              0x0               0x0               0x0
// 0x317C     LRFDRFE          IQIC                  [1]        AUTOCOEFFCLR          0x1            0x1           0x1              0x1               0x1               0x1
// 0x319C     LRFDRFE          RFECTL0               [15:8]     SPARE                 0x02           0x02          0x02             0x02              0x02              0x02
// 0x319C     LRFDRFE          RFECTL0               [7]        LDOREFBYP             0x0            0x0           0x0              0x0               0x0               0x0
// 0x319C     LRFDRFE          RFECTL0               [6]        VREFBP                0x0            0x0           0x0              0x0               0x0               0x0
// 0x319C     LRFDRFE          RFECTL0               [5]        TXWAITMOD             0x0            0x0           0x0              0x0               0x0               0x0
// 0x319C     LRFDRFE          RFECTL0               [4]        PHEDISC               0x1            0x1           0x1              0x1               0x1               0x1
// 0x319C     LRFDRFE          RFECTL0               [3]        RTRIMTST              0x0            0x0           0x0              0x0               0x0               0x0
// 0x319C     LRFDRFE          RFECTL0               [2]        IIR                   0x0            0x0           0x0              0x0               0x0               0x0
// 0x319C     LRFDRFE          RFECTL0               [1]        REFDTHR               0x1            0x1           0x1              0x1               0x1               0x1
// 0x319C     LRFDRFE          RFECTL0               [0]        FCDEM                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x31A0     LRFDRFE          RFECTL1               [4]        IQIC                  0x0            0x0           0x0              0x0               0x0               0x0
// 0x31A0     LRFDRFE          RFECTL1               [2]        TXOFDM                0x1            0x1           0x1              0x1               0x1               0x1
// 0x31A0     LRFDRFE          RFECTL1               [1]        RXCFG                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x31A0     LRFDRFE          RFECTL1               [0]        ULNA                  0x1            0x1           0x1              0x1               0x1               0x1
// 0x31BC     LRFDRFE          LNA                   [15:14]    RXTX                  0x1            0x1           0x1              0x1               0x1               0x1
// 0x31BC     LRFDRFE          LNA                   [11:8]     MIXCAP                0x0            0x2           0x3              0x3               0x8               0x8
// 0x31BC     LRFDRFE          LNA                   [7:4]      TRIM                  <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x31BC     LRFDRFE          LNA                   [3]        BIAS                  0x1            0x1           0x1              0x1               0x1               0x1
// 0x31BC     LRFDRFE          LNA                   [2:1]      IB                    0x1            0x1           0x1              0x1               0x1               0x1
// 0x31C0     LRFDRFE          IFAMP                 [15]       IFFBYP                0x0            0x0           0x0              0x0               0x0               0x0
// 0x31C0     LRFDRFE          IFAMP                 [14:12]    IFFDC                 <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x31C0     LRFDRFE          IFAMP                 [11:8]     IFFRES                <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x31C0     LRFDRFE          IFAMP                 [7:4]      IFFCAP                0x0            0x2           0x3              0x3               0x8               0x8
// 0x31C0     LRFDRFE          IFAMP                 [3:1]      IB                    0x3            0x3           0x3              0x3               0x3               0x3
// 0x31C8     LRFDRFE          PA                    [12:8]     TRIM                  <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x31C8     LRFDRFE          PA                    [7]        BIAS                  0x1            0x1           0x1              0x1               0x1               0x1
// 0x31C8     LRFDRFE          PA                    [6]        SHORT                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x31C8     LRFDRFE          PA                    [5:4]      RC                    0x0            0x0           0x0              0x0               0x0               0x0
// 0x31C8     LRFDRFE          PA                    [3:2]      AD                    0x1            0x1           0x1              0x1               0x1               0x1
// 0x31CC     LRFDRFE          ULNA                  [14]       TEMP                  0x0            0x0           0x0              0x0               0x0               0x0
// 0x31CC     LRFDRFE          ULNA                  [13]       LDOITST               0x0            0x0           0x0              0x0               0x0               0x0
// 0x31CC     LRFDRFE          ULNA                  [12]       LDOATST               0x0            0x0           0x0              0x0               0x0               0x0
// 0x31CC     LRFDRFE          ULNA                  [11]       MIXATST               0x0            0x0           0x0              0x0               0x0               0x0
// 0x31CC     LRFDRFE          ULNA                  [10]       ITST                  0x0            0x0           0x0              0x0               0x0               0x0
// 0x31CC     LRFDRFE          ULNA                  [9:8]      FBRES                 0x2            0x2           0x2              0x2               0x2               0x2
// 0x31CC     LRFDRFE          ULNA                  [7:4]      TRIM                  <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x31CC     LRFDRFE          ULNA                  [3:1]      IB                    0x3            0x3           0x3              0x3               0x3               0x3
// 0x31D0     LRFDRFE          RFLDO                 [10:8]     IFAMPATTN             0x0            0x0           0x0              0x0               0x0               0x0
// 0x31D0     LRFDRFE          RFLDO                 [7:1]      TRIM                  <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x31D4     LRFDRFE          IFADC0                [14:12]    DITHERTRIM            0x0            0x0           0x0              0x0               0x0               0x0
// 0x31D4     LRFDRFE          IFADC0                [11:10]    DITHEREN              0x0            0x0           0x0              0x0               0x0               0x0
// 0x31D4     LRFDRFE          IFADC0                [7:4]      INT2ADJ               <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x31D4     LRFDRFE          IFADC0                [1]        DTCICG                0x0            0x0           0x0              0x0               0x0               0x0
// 0x31D8     LRFDRFE          IFADC1                [15]       NRZ                   <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x31D8     LRFDRFE          IFADC1                [14:9]     TRIM                  <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x31D8     LRFDRFE          IFADC1                [4]        ADCLFSROUT            0x0            0x0           0x0              0x0               0x0               0x0
// 0x31D8     LRFDRFE          IFADC1                [3:1]      LPFTSTMODE            0x0            0x0           0x0              0x0               0x0               0x0
// 0x31D8     LRFDRFE          IFADC1                [0]        INVCLKOUT             0x0            0x0           0x0              0x0               0x0               0x0
// 0x31DC     LRFDRFE          IFADCLF               [15:12]    FF3                   <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x31DC     LRFDRFE          IFADCLF               [11:8]     FF2                   <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x31DC     LRFDRFE          IFADCLF               [7:4]      FF1                   <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x31DC     LRFDRFE          IFADCLF               [3:0]      INT3                  <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x31E0     LRFDRFE          IFADCQUANT            [15:14]    INT1ADJ               0x1            0x1           0x1              0x1               0x1               0x1
// 0x31E0     LRFDRFE          IFADCQUANT            [13:9]     DBGCALVALIN           0x00           0x00          0x00             0x00              0x00              0x00
// 0x31E0     LRFDRFE          IFADCQUANT            [8]        DBGCALLEG             0x0            0x0           0x0              0x0               0x0               0x0
// 0x31E0     LRFDRFE          IFADCQUANT            [7:6]      DBGCALMQ              0x0            0x0           0x0              0x0               0x0               0x0
// 0x31E0     LRFDRFE          IFADCQUANT            [5:4]      DBGCALMI              0x0            0x0           0x0              0x0               0x0               0x0
// 0x31E0     LRFDRFE          IFADCQUANT            [2:0]      QUANTTHR              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x31E4     LRFDRFE          IFADCALDO             [15]       ATESTVSSANA           0x0            0x0           0x0              0x0               0x0               0x0
// 0x31E4     LRFDRFE          IFADCALDO             [13:8]     TRIMOUT               <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x31E4     LRFDRFE          IFADCALDO             [7]        PASSCUR               0x0            0x0           0x0              0x0               0x0               0x0
// 0x31E4     LRFDRFE          IFADCALDO             [6]        ATESTOUT              0x0            0x0           0x0              0x0               0x0               0x0
// 0x31E4     LRFDRFE          IFADCALDO             [5]        ATESTINPUTREF         0x0            0x0           0x0              0x0               0x0               0x0
// 0x31E4     LRFDRFE          IFADCALDO             [4]        ATESTERRAMP           0x0            0x0           0x0              0x0               0x0               0x0
// 0x31E4     LRFDRFE          IFADCALDO             [3]        ITEST                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x31E4     LRFDRFE          IFADCALDO             [2]        BYPASS                0x0            0x0           0x0              0x0               0x0               0x0
// 0x31E4     LRFDRFE          IFADCALDO             [1]        CLAMP                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x31EC     LRFDRFE          IFADC2                [13]       INT3MODE              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x31EC     LRFDRFE          IFADC2                [12]       INT2MODE              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x31EC     LRFDRFE          IFADC2                [6]        QCALDBIQ              0x0            0x0           0x0              0x0               0x0               0x0
// 0x31EC     LRFDRFE          IFADC2                [5]        QCALDBC               0x0            0x0           0x0              0x0               0x0               0x0
// 0x31EC     LRFDRFE          IFADC2                [4:0]      SEL                   0x00           0x00          0x00             0x00              0x00              0x00
// 0x31F4     LRFDRFE          ATSTREF0              [15:0]     MUX                   0x0000         0x0000        0x0000           0x0000            0x0000            0x0000
// 0x31F8     LRFDRFE          ATSTREF1              [15]       VREFBPDIS             0x0            0x0           0x0              0x0               0x0               0x0
// 0x31F8     LRFDRFE          ATSTREF1              [14:10]    IREFTRIM              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x31F8     LRFDRFE          ATSTREF1              [8]        OUTPUT2               0x0            0x0           0x0              0x0               0x0               0x0
// 0x31F8     LRFDRFE          ATSTREF1              [7]        OUTPUT1               0x0            0x0           0x0              0x0               0x0               0x0
// 0x31FC     LRFDRFE          ATSTREF2              [15:0]     MUX                   0x0000         0x0000        0x0000           0x0000            0x0000            0x0000
// 0x3200     LRFDRFE          DIVCTL                [11]       TXBBOOST              0x1            0x1           0x1              0x1               0x1               0x1
// 0x3200     LRFDRFE          DIVCTL                [10]       IFADCCLKFREQ          0x0            0x0           0x0              0x0               0x0               0x0
// 0x3204     LRFDRFE          DIV                   [15]       PDET                  0x0            0x0           0x0              0x0               0x0               0x0
// 0x3204     LRFDRFE          DIV                   [14:12]    NMIREFTRIM            0x0            0x0           0x0              0x0               0x0               0x0
// 0x3204     LRFDRFE          DIV                   [11:9]     PMIREFTRIM            0x0            0x0           0x0              0x0               0x0               0x0
// 0x3204     LRFDRFE          DIV                   [8:0]      RATIO                 0x104          0x104         0x104            0x104             0x104             0x104
// 0x3208     LRFDRFE          DIVLDO                [15]       ITEST                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x3208     LRFDRFE          DIVLDO                [14:8]     VOUTTRIM              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x3208     LRFDRFE          DIVLDO                [7]        FASTLOOP              0x0            0x0           0x0              0x0               0x0               0x0
// 0x3208     LRFDRFE          DIVLDO                [6:4]      TMUX                  0x0            0x0           0x0              0x0               0x0               0x0
// 0x3208     LRFDRFE          DIVLDO                [3]        PDSEL                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x3208     LRFDRFE          DIVLDO                [2]        MODE                  0x0            0x0           0x0              0x0               0x0               0x0
// 0x3208     LRFDRFE          DIVLDO                [1]        BYPASS                0x0            0x0           0x0              0x0               0x0               0x0
// 0x320C     LRFDRFE          TDCLDO                [15]       ITEST                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x320C     LRFDRFE          TDCLDO                [14:8]     VOUTTRIM              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x320C     LRFDRFE          TDCLDO                [7]        FASTLOOP              0x0            0x0           0x0              0x0               0x0               0x0
// 0x320C     LRFDRFE          TDCLDO                [6:4]      TMUX                  0x0            0x0           0x0              0x0               0x0               0x0
// 0x320C     LRFDRFE          TDCLDO                [3]        PDSEL                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x320C     LRFDRFE          TDCLDO                [2]        MODE                  0x0            0x0           0x0              0x0               0x0               0x0
// 0x320C     LRFDRFE          TDCLDO                [1]        BYPASS                0x0            0x0           0x0              0x0               0x0               0x0
// 0x3210     LRFDRFE          DCOLDO0               [15:8]     VOUTTRIM              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x3210     LRFDRFE          DCOLDO0               [7]        COMPCAP               0x0            0x0           0x0              0x0               0x0               0x0
// 0x3210     LRFDRFE          DCOLDO0               [6]        CURRENT               0x0            0x0           0x0              0x0               0x0               0x0
// 0x3210     LRFDRFE          DCOLDO0               [5]        ITEST                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x3210     LRFDRFE          DCOLDO0               [4]        VTEST                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x3210     LRFDRFE          DCOLDO0               [3]        LDOBYP                0x0            0x0           0x0              0x0               0x0               0x0
// 0x3210     LRFDRFE          DCOLDO0               [2]        LDOREFBYP             0x0            0x0           0x0              0x0               0x0               0x0
// 0x3210     LRFDRFE          DCOLDO0               [1]        SUPPLY                0x1            0x1           0x1              0x1               0x1               0x1
// 0x3214     LRFDRFE          DCOLDO1               [11]       PEAKDETATST           0x0            0x0           0x0              0x0               0x0               0x0
// 0x3214     LRFDRFE          DCOLDO1               [10]       REFSRC                0x0            0x0           0x0              0x0               0x0               0x0
// 0x3214     LRFDRFE          DCOLDO1               [9:8]      DIVATST               0x0            0x0           0x0              0x0               0x0               0x0
// 0x3218     LRFDRFE          DCO                   [8]        LATCHRSTEN            0x0            0x0           0x0              0x0               0x0               0x0
// 0x3218     LRFDRFE          DCO                   [6:3]      TAILRESTRIM           <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x3218     LRFDRFE          DCO                   [1]        HFOVR                 0x1            0x1           0x1              0x1               0x1               0x1
// 0x3218     LRFDRFE          DCO                   [0]        CRSCAPCM              0x1            0x1           0x1              0x1               0x1               0x1
// 0x321C     LRFDRFE          DIVPDET               [15]       RBIAS2                0x0            0x0           0x0              0x0               0x0               0x0
// 0x321C     LRFDRFE          DIVPDET               [14:12]    JCNTFREQ              0x0            0x0           0x0              0x0               0x0               0x0
// 0x321C     LRFDRFE          DIVPDET               [11:9]     JCNTPH                0x0            0x0           0x0              0x0               0x0               0x0
// 0x321C     LRFDRFE          DIVPDET               [8:7]      RBIAS10               0x0            0x0           0x0              0x0               0x0               0x0
// 0x321C     LRFDRFE          DIVPDET               [6]        PHOVR                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x321C     LRFDRFE          DIVPDET               [5:3]      REF                   <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x3220     LRFDRFE          TXOFDM0               [15]       IREF2U                0x0            0x0           0x0              0x0               0x0               0x0
// 0x3220     LRFDRFE          TXOFDM0               [14]       IREF20U               0x1            0x1           0x1              0x1               0x1               0x1
// 0x3224     LRFDRFE          TXOFDM1               [15:12]    CMFBIREF              0x5            0x5           0x5              0x5               0x5               0x5
// 0x3224     LRFDRFE          TXOFDM1               [11:8]     PABIASIREF            0x5            0x5           0x5              0x5               0x5               0x5
// 0x3224     LRFDRFE          TXOFDM1               [7:4]      FILTIREF              0x5            0x5           0x5              0x5               0x5               0x5
// 0x3228     LRFDRFE          TXOFDM2               [15:12]    PPA2IREF              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x3228     LRFDRFE          TXOFDM2               [11:8]     PPA1IREF              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x3228     LRFDRFE          TXOFDM2               [7:4]      PAFIXBIASIREF         <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x3228     LRFDRFE          TXOFDM2               [3:0]      PPAFSKIREF            0x5            0x5           0x5              0x5               0x5               0x5
// 0x322C     LRFDRFE          TXOFDM3               [15]       ITESTBIASSEL          0x0            0x0           0x0              0x0               0x0               0x0
// 0x322C     LRFDRFE          TXOFDM3               [14]       PPA2BIASSEL           0x0            0x0           0x0              0x0               0x0               0x0
// 0x322C     LRFDRFE          TXOFDM3               [13]       PPA1BIASSEL           0x0            0x0           0x0              0x0               0x0               0x0
// 0x322C     LRFDRFE          TXOFDM3               [12]       PAFIXBIASSEL          0x0            0x0           0x0              0x0               0x0               0x0
// 0x322C     LRFDRFE          TXOFDM3               [11]       ITRIMSWEN             0x0            0x0           0x0              0x0               0x0               0x0
// 0x322C     LRFDRFE          TXOFDM3               [10]       RESSWEN               0x0            0x0           0x0              0x0               0x0               0x0
// 0x322C     LRFDRFE          TXOFDM3               [9]        GNDSWEN               0x0            0x0           0x0              0x0               0x0               0x0
// 0x322C     LRFDRFE          TXOFDM3               [4]        DACLSB                0x1            0x1           0x1              0x1               0x1               0x1
// 0x322C     LRFDRFE          TXOFDM3               [3]        DACCLKINV             0x0            0x0           0x0              0x0               0x0               0x0
// 0x3230     LRFDRFE          TXOFDM4               [10:9]     FILTIOUT              0x1            0x1           0x1              0x1               0x1               0x1
// 0x3230     LRFDRFE          TXOFDM4               [8:6]      FILTPH90ITRIM         0x5            0x5           0x5              0x5               0x5               0x5
// 0x3230     LRFDRFE          TXOFDM4               [5:3]      FILTPH0ITRIM          0x5            0x5           0x5              0x5               0x5               0x5
// 0x3230     LRFDRFE          TXOFDM4               [2:0]      FILTVCMTRIM           <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x3234     LRFDRFE          TXOFDM5               [14:9]     FILTFBRTRIM           <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x3234     LRFDRFE          TXOFDM5               [8:3]      FILTINRTRIM           <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x3234     LRFDRFE          TXOFDM5               [2:0]      FILTRTRIM             <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x3238     LRFDRFE          TXOFDM6               [14:12]    BUFPH90ITRIM          0x4            0x4           0x4              0x4               0x4               0x4
// 0x3238     LRFDRFE          TXOFDM6               [11:9]     BUFPH0ITRIM           0x4            0x4           0x4              0x4               0x4               0x4
// 0x3238     LRFDRFE          TXOFDM6               [8:6]      FILTPH90FBCAP         0x3            0x3           0x3              0x3               0x3               0x3
// 0x3238     LRFDRFE          TXOFDM6               [5:3]      FILTPH0FBCAP          0x3            0x3           0x3              0x3               0x3               0x3
// 0x323C     LRFDRFE          TXOFDM7               [15:14]    CMFBGAIN              0x2            0x2           0x2              0x2               0x2               0x2
// 0x323C     LRFDRFE          TXOFDM7               [13]       PPA1HP                0x0            0x0           0x0              0x0               0x0               0x0
// 0x323C     LRFDRFE          TXOFDM7               [12:11]    PPAFSKATTN            0x0            0x0           0x0              0x0               0x0               0x0
// 0x323C     LRFDRFE          TXOFDM7               [10:8]     PPA1RTRIM             0x0            0x0           0x0              0x0               0x0               0x0
// 0x323C     LRFDRFE          TXOFDM7               [7:6]      PPA1PH90GAIN          0x1            0x1           0x1              0x1               0x1               0x1
// 0x323C     LRFDRFE          TXOFDM7               [5:4]      PPA1PH0GAIN           0x1            0x1           0x1              0x1               0x1               0x1
// 0x323C     LRFDRFE          TXOFDM7               [3:0]      MIXDCBIAS             0x4            0x4           0x4              0x4               0x4               0x4
// 0x3240     LRFDRFE          TXOFDM8               [6]        PPA2HP                0x0            0x0           0x0              0x0               0x0               0x0
// 0x3240     LRFDRFE          TXOFDM8               [5:2]      PPA2RTRIM             0x0            0x0           0x0              0x0               0x0               0x0
// 0x3240     LRFDRFE          TXOFDM8               [1:0]      PPA2GAIN              0x2            0x2           0x2              0x2               0x2               0x2
// 0x3244     LRFDRFE          TXOFDM9               [13]       ENVDREF               0x0            0x0           0x0              0x0               0x0               0x0
// 0x3244     LRFDRFE          TXOFDM9               [12:9]     ENVDPBLDR             0x0            0x0           0x0              0x0               0x0               0x0
// 0x3244     LRFDRFE          TXOFDM9               [8:5]      ENVDNBLDR             0x0            0x0           0x0              0x0               0x0               0x0
// 0x3244     LRFDRFE          TXOFDM9               [4:0]      ENVDNBIAS             0x10           0x10          0x10             0x10              0x10              0x10
// 0x3248     LRFDRFE          TXOFDM10              [15]       PAFIXBIAS             0x1            0x1           0x1              0x1               0x1               0x1
// 0x3248     LRFDRFE          TXOFDM10              [14]       ENVDBIAS              0x0            0x0           0x0              0x0               0x0               0x0
// 0x3248     LRFDRFE          TXOFDM10              [13:8]     PAFIXBIASVREF         0x26           0x26          0x26             0x26              0x26              0x26
// 0x3248     LRFDRFE          TXOFDM10              [7:4]      ENVDVREF              0x6            0x6           0x6              0x6               0x6               0x6
// 0x3248     LRFDRFE          TXOFDM10              [3:0]      ENVDIREF              0x6            0x6           0x6              0x6               0x6               0x6
// 0x324C     LRFDRFE          TXOFDM11              [14:13]    PPA2RBIASMSB          0x0            0x0           0x0              0x0               0x0               0x0
// 0x324C     LRFDRFE          TXOFDM11              [12:11]    PPA2RBIASLSB          0x0            0x0           0x0              0x0               0x0               0x0
// 0x324C     LRFDRFE          TXOFDM11              [6:4]      PACASLOW              0x2            0x2           0x2              0x2               0x2               0x2
// 0x324C     LRFDRFE          TXOFDM11              [3]        PACASSHRT             0x0            0x0           0x0              0x0               0x0               0x0
// 0x324C     LRFDRFE          TXOFDM11              [2:0]      PACASHGH              0x2            0x2           0x2              0x2               0x2               0x2
// 0x3250     LRFDRFE          TXOFDM12              [15]       PATEMP                0x0            0x0           0x0              0x0               0x0               0x0
// 0x3250     LRFDRFE          TXOFDM12              [8:0]      TESTCTL               0x000          0x000         0x000            0x000             0x000             0x000
// 0x3254     LRFDRFE          TXOFDM13              [15:14]    SPARE14               0x1            0x1           0x1              0x1               0x1               0x1
// 0x3254     LRFDRFE          TXOFDM13              [13:11]    PAPDETIREF            0x2            0x2           0x2              0x2               0x2               0x2
// 0x3254     LRFDRFE          TXOFDM13              [10:9]     PAPDETGAIN            0x0            0x0           0x0              0x0               0x0               0x0
// 0x3254     LRFDRFE          TXOFDM13              [8]        PAPDET                0x0            0x0           0x0              0x0               0x0               0x0
// 0x3254     LRFDRFE          TXOFDM13              [7:6]      SPARE6                0x1            0x1           0x1              0x1               0x1               0x1
// 0x3254     LRFDRFE          TXOFDM13              [5:3]      PPA2PDETRTRIM         <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x3254     LRFDRFE          TXOFDM13              [2:1]      PPA2PDETIREF          0x1            0x1           0x1              0x1               0x1               0x1
// 0x3254     LRFDRFE          TXOFDM13              [0]        PPA2PDET              0x0            0x0           0x0              0x0               0x0               0x0
// 0x3258     LRFDRFE          TXOFDM14              [15:14]    FSKRAMPCAP            0x0            0x0           0x0              0x0               0x0               0x0
// 0x3258     LRFDRFE          TXOFDM14              [13:11]    FSKRAMPIREF           0x0            0x0           0x0              0x0               0x0               0x0
// 0x3258     LRFDRFE          TXOFDM14              [10:9]     FSKRAMPIDIS           0x0            0x0           0x0              0x0               0x0               0x0
// 0x3258     LRFDRFE          TXOFDM14              [8]        FSKRAMPDNCTL          0x0            0x0           0x0              0x0               0x0               0x0
// 0x3258     LRFDRFE          TXOFDM14              [7:6]      FSKRAMPITAIL          0x0            0x0           0x0              0x0               0x0               0x0
// 0x3258     LRFDRFE          TXOFDM14              [5]        BIASGENCON            0x1            0x1           0x1              0x1               0x1               0x1
// 0x3258     LRFDRFE          TXOFDM14              [4]        FSKRAMPOUT            0x0            0x0           0x0              0x0               0x0               0x0
// 0x3258     LRFDRFE          TXOFDM14              [3]        FSKRAMPBYP            0x1            0x1           0x1              0x1               0x1               0x1
// 0x3258     LRFDRFE          TXOFDM14              [2]        FSKRAMPOUTPD          0x0            0x0           0x0              0x0               0x0               0x0
// 0x3258     LRFDRFE          TXOFDM14              [1]        FSKRAMPIN             0x0            0x0           0x0              0x0               0x0               0x0
// 0x3258     LRFDRFE          TXOFDM14              [0]        FSKRAMPCTL            0x0            0x0           0x0              0x0               0x0               0x0
// 0x3260     LRFDRFE          TXOFDMLDO             [15]       ITEST                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x3260     LRFDRFE          TXOFDMLDO             [14:8]     VOUTTRIM              <TRIM>         <TRIM>        <TRIM>           <TRIM>            <TRIM>            <TRIM>
// 0x3260     LRFDRFE          TXOFDMLDO             [7]        FASTLOOP              0x0            0x0           0x0              0x0               0x0               0x0
// 0x3260     LRFDRFE          TXOFDMLDO             [6:4]      TMUX                  0x0            0x0           0x0              0x0               0x0               0x0
// 0x3260     LRFDRFE          TXOFDMLDO             [3]        PDSEL                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x3260     LRFDRFE          TXOFDMLDO             [2]        MODE                  0x0            0x0           0x0              0x0               0x0               0x0
// 0x3260     LRFDRFE          TXOFDMLDO             [1]        BYPASS                0x0            0x0           0x0              0x0               0x0               0x0
// 0x3268     LRFDRFE          PRE0                  [13:8]     PLLDIV1               0x08           0x08          0x08             0x08              0x08              0x08
// 0x3268     LRFDRFE          PRE0                  [5:0]      PLLDIV0               0x07           0x07          0x07             0x07              0x07              0x07
// 0x326C     LRFDRFE          PRE1                  [5:0]      HSDDC                 0x00           0x00          0x00             0x00              0x00              0x00
// 0x3270     LRFDRFE          PRE2                  [15:12]    MIDCALDIVLSB          0x0            0x0           0x0              0x0               0x0               0x0
// 0x3270     LRFDRFE          PRE2                  [11:6]     CRSCALDIV             0x18           0x18          0x18             0x18              0x18              0x18
// 0x3270     LRFDRFE          PRE2                  [5:0]      FSMDIV                0x05           0x05          0x05             0x05              0x05              0x05
// 0x3274     LRFDRFE          PRE3                  [15:5]     FINECALDIV            0x030          0x030         0x030            0x030             0x030             0x030
// 0x3274     LRFDRFE          PRE3                  [4:0]      MIDCALDIVMSB          0x03           0x03          0x03             0x03              0x03              0x03
// 0x3278     LRFDRFE          CAL0                  [14:8]     FCSTART               0x40           0x40          0x40             0x40              0x40              0x40
// 0x3278     LRFDRFE          CAL0                  [3:2]      TDCAVG                0x2            0x2           0x2              0x2               0x2               0x2
// 0x327C     LRFDRFE          CAL1                  [14:8]     FCTOP                 0x7F           0x7F          0x7F             0x7F              0x7F              0x7F
// 0x327C     LRFDRFE          CAL1                  [6:0]      FCBOT                 0x00           0x00          0x00             0x00              0x00              0x00
// 0x3288     LRFDRFE          MISC0                 [15]       TDCCALENHCTL          0x1            0x1           0x1              0x1               0x1               0x1
// 0x3288     LRFDRFE          MISC0                 [14]       TDCCALENHCFG          0x1            0x1           0x1              0x1               0x1               0x1
// 0x3288     LRFDRFE          MISC0                 [13]       PHCPT                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x3288     LRFDRFE          MISC0                 [12]       TDCCALCORR            0x1            0x1           0x1              0x1               0x1               0x1
// 0x3288     LRFDRFE          MISC0                 [11]       TDCMSBCORR            0x1            0x1           0x1              0x1               0x1               0x1
// 0x3288     LRFDRFE          MISC0                 [5:4]      DLYCANCRS             0x2            0x2           0x2              0x2               0x2               0x2
// 0x3298     LRFDRFE          PHEDISC               [13:10]    CNT                   0x3            0x3           0x3              0x3               0x3               0x3
// 0x3298     LRFDRFE          PHEDISC               [9:0]      THR                   0x007          0x007         0x007            0x007             0x007             0x007
// 0x32A0     LRFDRFE          PLLMON0               [15:14]    PHELOLCNT             0x3            0x3           0x3              0x3               0x3               0x3
// 0x32A0     LRFDRFE          PLLMON0               [13:8]     PHELOLTHR             0x1F           0x1F          0x1F             0x1F              0x1F              0x1F
// 0x32A0     LRFDRFE          PLLMON0               [6:0]      FCTHR                 0x7F           0x7F          0x7F             0x7F              0x7F              0x7F
// 0x32A4     LRFDRFE          PLLMON1               [12:8]     PHELOCKCNT            0x04           0x04          0x04             0x04              0x04              0x04
// 0x32A4     LRFDRFE          PLLMON1               [7:0]      PHELOCKTHR            0x10           0x10          0x10             0x10              0x10              0x10
// 0x3370     LRFDRFE          DCOHFT0               [15:12]    C3                    0x3            0x3           0x3              0x3               0x3               0x3
// 0x3370     LRFDRFE          DCOHFT0               [11:8]     C2                    0x2            0x2           0x2              0x2               0x2               0x2
// 0x3370     LRFDRFE          DCOHFT0               [7:4]      C1                    0x1            0x1           0x1              0x1               0x1               0x1
// 0x3370     LRFDRFE          DCOHFT0               [3:0]      C0                    0x0            0x0           0x0              0x0               0x0               0x0
// 0x3374     LRFDRFE          DCOHFT1               [15:12]    C7                    0x7            0x7           0x7              0x7               0x7               0x7
// 0x3374     LRFDRFE          DCOHFT1               [11:8]     C6                    0x6            0x6           0x6              0x6               0x6               0x6
// 0x3374     LRFDRFE          DCOHFT1               [7:4]      C5                    0x5            0x5           0x5              0x5               0x5               0x5
// 0x3374     LRFDRFE          DCOHFT1               [3:0]      C4                    0x4            0x4           0x4              0x4               0x4               0x4
// 0x3378     LRFDRFE          DCOHFT2               [15:12]    C11                   0xB            0xB           0xB              0xB               0xB               0xB
// 0x3378     LRFDRFE          DCOHFT2               [11:8]     C10                   0xA            0xA           0xA              0xA               0xA               0xA
// 0x3378     LRFDRFE          DCOHFT2               [7:4]      C9                    0x9            0x9           0x9              0x9               0x9               0x9
// 0x3378     LRFDRFE          DCOHFT2               [3:0]      C8                    0x8            0x8           0x8              0x8               0x8               0x8
// 0x337C     LRFDRFE          DCOHFT3               [15:12]    C15                   0xF            0xF           0xF              0xF               0xF               0xF
// 0x337C     LRFDRFE          DCOHFT3               [11:8]     C14                   0xE            0xE           0xE              0xE               0xE               0xE
// 0x337C     LRFDRFE          DCOHFT3               [7:4]      C13                   0xD            0xD           0xD              0xD               0xD               0xD
// 0x337C     LRFDRFE          DCOHFT3               [3:0]      C12                   0xC            0xC           0xC              0xC               0xC               0xC
// 0x3380     LRFDRFE          DCOHFB0               [15:12]    C3                    0x3            0x3           0x3              0x3               0x3               0x3
// 0x3380     LRFDRFE          DCOHFB0               [11:8]     C2                    0x2            0x2           0x2              0x2               0x2               0x2
// 0x3380     LRFDRFE          DCOHFB0               [7:4]      C1                    0x1            0x1           0x1              0x1               0x1               0x1
// 0x3380     LRFDRFE          DCOHFB0               [3:0]      C0                    0x0            0x0           0x0              0x0               0x0               0x0
// 0x3384     LRFDRFE          DCOHFB1               [15:12]    C7                    0x7            0x7           0x7              0x7               0x7               0x7
// 0x3384     LRFDRFE          DCOHFB1               [11:8]     C6                    0x6            0x6           0x6              0x6               0x6               0x6
// 0x3384     LRFDRFE          DCOHFB1               [7:4]      C5                    0x5            0x5           0x5              0x5               0x5               0x5
// 0x3384     LRFDRFE          DCOHFB1               [3:0]      C4                    0x4            0x4           0x4              0x4               0x4               0x4
// 0x3388     LRFDRFE          DCOHFB2               [15:12]    C11                   0xB            0xB           0xB              0xB               0xB               0xB
// 0x3388     LRFDRFE          DCOHFB2               [11:8]     C10                   0xA            0xA           0xA              0xA               0xA               0xA
// 0x3388     LRFDRFE          DCOHFB2               [7:4]      C9                    0x9            0x9           0x9              0x9               0x9               0x9
// 0x3388     LRFDRFE          DCOHFB2               [3:0]      C8                    0x8            0x8           0x8              0x8               0x8               0x8
// 0x338C     LRFDRFE          DCOHFB3               [15:12]    C15                   0xF            0xF           0xF              0xF               0xF               0xF
// 0x338C     LRFDRFE          DCOHFB3               [11:8]     C14                   0xE            0xE           0xE              0xE               0xE               0xE
// 0x338C     LRFDRFE          DCOHFB3               [7:4]      C13                   0xD            0xD           0xD              0xD               0xD               0xD
// 0x338C     LRFDRFE          DCOHFB3               [3:0]      C12                   0xC            0xC           0xC              0xC               0xC               0xC
// 0x3390     LRFDRFE          SPARE0                [15:0]     VAL                   0x3090         0x3090        0x50B0           0x50B0            0x50B0            0x50B0
// 0x3394     RFE_FW           PHYRX0RSSI            [7:0]      OFFSET                0x47           0x46          0x4C             0x4C              0x4B              0x4B
// 0x3398     RFE_FW           PHYRX1RSSI            [7:0]      OFFSET                0x47           0x46          0x4C             0x4C              0x4B              0x4B
// 0x339C     RFE_FW           PHYAGCCFG4            [15:8]     NOSIGNALTHR           0x24           0x12          0x12             0x12              0x16              0x16
// 0x339C     RFE_FW           PHYAGCCFG4            [7:0]      AGCIRQTHR             0x2D           0x28          0x2C             0x2C              0x26              0x29
// 0x33A0     RFE_FW           RXCODCCFG2            [7:5]      IIRGAINUSLOW          0x6            0x6           0x6              0x6               0x6               0x6
// 0x33A0     RFE_FW           RXCODCCFG2            [4]        IIRENUSLOW            0x1            0x1           0x1              0x1               0x1               0x1
// 0x33A0     RFE_FW           RXCODCCFG2            [3]        ACCMODEUSLOW          0x0            0x0           0x0              0x0               0x0               0x0
// 0x33A0     RFE_FW           RXCODCCFG2            [2:1]      ACCPERUSLOW           0x0            0x0           0x0              0x0               0x0               0x0
// 0x33A0     RFE_FW           RXCODCCFG2            [0]        ACCENUSLOW            0x0            0x0           0x0              0x0               0x0               0x0
// 0x33A4     RFE_FW           RXFRONTEND            [13:12]    PINSEL                0x2            0x2           0x2              0x2               0x2               0x2
// 0x33A8     RFE_FW           AGCCFG5               [7:0]      DELTAGCREF            0x02           0x03          0x03             0x03              0x03              0x03
// 0x33AC     RFE_FW           IFADCPEAKTHR          [7:0]      THR                   0x0A           0x08          0x08             0x08              0x08              0x08
// 0x33B8     RFE_FW           FEDELTARSSI           [15:0]     OFFSET                0x0000         0x0000        0x0000           0x0000            0x0000            0x0000
// 0x33BC     RFE_FW           FEDELTANOISEFIGURE    [15:0]     VAL                   0x0000         0x0000        0x0000           0x0000            0x0000            0x0000
// 0x33C0     RFE_FW           AGCCFG6               [15:0]     SLOPETHR              0x001C         0x001C        0x001C           0x001C            0x001C            0x001C
// 0x33C8     RFE_FW           AGCCFG8               [15:0]     STOPCLIMBTHR          0x0055         0x0055        0x0055           0x0055            0x004F            0x004F
// 0x60BC     LRFDOCE          MDM1                  [3]        LRRX1                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x60BC     LRFDOCE          MDM1                  [2]        LRRX0                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x60BC     LRFDOCE          MDM1                  [1]        SUNRX1                0x0            0x0           0x0              0x0               0x0               0x0
// 0x60BC     LRFDOCE          MDM1                  [0]        SUNRX0                0x1            0x1           0x1              0x1               0x1               0x1
// 0x60C4     LRFDOCE          SUNFFT0               [15]       DEMGAIN               0x1            0x1           0x1              0x1               0x1               0x1
// 0x60C4     LRFDOCE          SUNFFT0               [14:12]    MODGAIN               0x3            0x2           0x3              0x3               0x3               0x3
// 0x60C4     LRFDOCE          SUNFFT0               [11:10]    NDT                   0x0            0x1           0x2              0x2               0x3               0x3
// 0x60C4     LRFDOCE          SUNFFT0               [9:8]      RXDFTSZ               0x0            0x0           0x0              0x0               0x0               0x0
// 0x60C4     LRFDOCE          SUNFFT0               [7:4]      IFFTGAIN              0x3            0x4           0x2              0x2               0x2               0x2
// 0x60C4     LRFDOCE          SUNFFT0               [3:0]      FFTGAIN               0x8            0x7           0x6              0x6               0x7               0x7
// 0x60D0     LRFDOCE          SUNMOD0               [15:11]    MODFINEGAIN           0x1A           0x0A          0x1B             0x1B              0x0C              0x0C
// 0x60D0     LRFDOCE          SUNMOD0               [10:9]     TXDFTSZ               0x0            0x1           0x2              0x2               0x3               0x3
// 0x60D0     LRFDOCE          SUNMOD0               [8:0]      TXINVIQFIRST          0x05F          0x02F         0x017            0x017             0x007             0x007
// 0x60D4     LRFDOCE          SUNDEM0               [15:0]     RXSIGNALREF           0xC400         0x6900        0x3580           0x3580            0x5200            0x5200
// 0x60D8     LRFDOCE          SUNDEM1               [14:8]     RXFIRSTDATASAMPL      0x21           0x21          0x21             0x21              0x21              0x21
// 0x60D8     LRFDOCE          SUNDEM1               [7:0]      RXFIRSTLTFSAMPL       0x00           0x00          0x00             0x00              0x00              0x00
// 0x60DC     LRFDOCE          SUNDEM2               [14]       INVPOWERFREEZEEN      0x1            0x1           0x1              0x1               0x1               0x1
// 0x60DC     LRFDOCE          SUNDEM2               [13]       RXNORMMETHOD          0x0            0x0           0x0              0x0               0x0               0x0
// 0x60DC     LRFDOCE          SUNDEM2               [12:11]    RXUPSAMPLE            0x0            0x1           0x2              0x2               0x2               0x2
// 0x60DC     LRFDOCE          SUNDEM2               [8]        NORMCORRDIS           0x0            0x0           0x0              0x0               0x0               0x0
// 0x60DC     LRFDOCE          SUNDEM2               [7]        NORMFREEZE            0x0            0x0           0x0              0x0               0x0               0x0
// 0x60DC     LRFDOCE          SUNDEM2               [6:4]      NORMOFFSET            0x0            0x0           0x0              0x0               0x0               0x0
// 0x60DC     LRFDOCE          SUNDEM2               [3:0]      NORMPERIOD            0x6            0x4           0x6              0x6               0x4               0x4
// 0x60E8     LRFDOCE          SUNCORR0              [11]       STOPCORRBELOWTHR      0x1            0x1           0x1              0x1               0x1               0x1
// 0x60E8     LRFDOCE          SUNCORR0              [10:9]     MAXPEAKDECAY          0x0            0x0           0x0              0x0               0x0               0x0
// 0x60E8     LRFDOCE          SUNCORR0              [8]        STF1SPOSEDGETHR       0x0            0x0           0x0              0x0               0x0               0x0
// 0x60E8     LRFDOCE          SUNCORR0              [7]        STF1STHR              0x0            0x0           0x0              0x0               0x0               0x0
// 0x60E8     LRFDOCE          SUNCORR0              [6]        STOPCORR              0x0            0x0           0x0              0x0               0x0               0x0
// 0x60E8     LRFDOCE          SUNCORR0              [5]        HWCFOEN               0x1            0x1           0x1              0x1               0x1               0x1
// 0x60E8     LRFDOCE          SUNCORR0              [4]        NOTONEQUAL            0x1            0x1           0x1              0x1               0x1               0x1
// 0x60E8     LRFDOCE          SUNCORR0              [3]        CLEARBESTPEAK         0x0            0x0           0x0              0x0               0x0               0x0
// 0x60E8     LRFDOCE          SUNCORR0              [2]        SUPREARLYSYNCEN       0x1            0x1           0x1              0x1               0x1               0x1
// 0x60E8     LRFDOCE          SUNCORR0              [1:0]      S9WEIGTH              0x1            0x1           0x1              0x1               0x1               0x1
// 0x60F4     LRFDOCE          SUNCORR3              [11:0]     STFAVGTHR             0x800          0x800         0x800            0x800             0x800             0x800
// 0x60F8     LRFDOCE          SUNPILOT0             [15:8]     BETA                  0x1F           0x1F          0x1F             0x1F              0x1F              0x1F
// 0x60F8     LRFDOCE          SUNPILOT0             [7:0]      ALPHA                 0x04           0x04          0x04             0x04              0x04              0x04
// 0x60FC     LRFDOCE          SUNPILOT1             [15:14]    CHFIBW                0x0            0x1           0x2              0x2               0x3               0x3
// 0x60FC     LRFDOCE          SUNPILOT1             [13:12]    TERRG                 0x1            0x1           0x1              0x1               0x1               0x1
// 0x60FC     LRFDOCE          SUNPILOT1             [11:9]     TERRPER1              0x2            0x2           0x2              0x2               0x2               0x2
// 0x60FC     LRFDOCE          SUNPILOT1             [8:6]      TERRPERN              0x3            0x3           0x3              0x3               0x3               0x3
// 0x60FC     LRFDOCE          SUNPILOT1             [4:3]      IIRBW                 0x2            0x2           0x2              0x2               0x2               0x2
// 0x60FC     LRFDOCE          SUNPILOT1             [2:0]      MU                    0x3            0x3           0x3              0x3               0x3               0x3
// 0x6110     LRFDOCE          SUNCORR4              [14:8]     STF_NORMCORRLVL       0x43           0x43          0x43             0x43              0x43              0x43
// 0x6110     LRFDOCE          SUNCORR4              [7]        POWERSAMPLELEN        0x0            0x1           0x1              0x1               0x1               0x1
// 0x6110     LRFDOCE          SUNCORR4              [5]        WIDEPEAKQUAL          0x1            0x1           0x1              0x1               0x1               0x1
// 0x6110     LRFDOCE          SUNCORR4              [4:0]      WIDEPEAKTHR           0x0F           0x0F          0x0F             0x0F              0x1A              0x16
// 0x6168     OCE_MAIN_FW      FWCFG4                [8]        PHRRESYNC             0x1            0x1           0x1              0x1               0x1               0x1
// 0x6178     OCE_MAIN_FW      FWCFG2                [15:12]    SPERIOD               0x5            0x4           0x4              0x4               0x5               0x5
// 0x6178     OCE_MAIN_FW      FWCFG2                [11:4]     PHR_LEN               0x03           0x06          0x0C             0x06              0x18              0x06
// 0x6178     OCE_MAIN_FW      FWCFG2                [3]        PPTGEARING            0x1            0x1           0x1              0x1               0x1               0x1
// 0x6178     OCE_MAIN_FW      FWCFG2                [2]        SUNCLEARBESTPEAK      0x1            0x1           0x1              0x1               0x1               0x1
// 0x6178     OCE_MAIN_FW      FWCFG2                [1:0]      PHR_MCS               0x0            0x0           0x0              0x1               0x0               0x2
// 0x61AC     OCE_MAIN_FW      FWCFG3                [12:2]     DELAYCLEARBESTPEAK    -              -             -                -                 0x7C0             0x7C0
// 0x61AC     OCE_MAIN_FW      FWCFG3                [0]        NOSYNC                0x0            0x0           0x0              0x0               0x0               0x0
// 0x6220     LRFDOCE          IFFTSAT               [8:0]      VAL                   0x000          0x000         0x000            0x000             -                 -
// 0x0018     PBE_RAM          CSCTL2DEFAULT         [15:8]     RSSITHRESH            0xA1           0xA1          0xA1             0xA1              0xA1              0xA1
// 0x0018     PBE_RAM          CSCTL2DEFAULT         [7:4]      NUMRSSIHITSBUSY       0x4            0x4           0x4              0x4               0x4               0x4
// 0x0018     PBE_RAM          CSCTL2DEFAULT         [3:0]      RSSIWINDOW            0x8            0x8           0x8              0x8               0x8               0x8
// 0x001A     PBE_RAM          CSCTL3DEFAULT         [15:8]     PDPERIOD              0x05           0x05          0x05             0x05              0x05              0x05
// 0x001A     PBE_RAM          CSCTL3DEFAULT         [7:4]      NUMPDHITSBUSY         0x1            0x1           0x1              0x1               0x1               0x1
// 0x001A     PBE_RAM          CSCTL3DEFAULT         [3:0]      PDWINDOW              0x8            0x8           0x8              0x8               0x8               0x8
// 0x6804     RFE_RAM          TDCCAL0               [10:8]     STOP                  0x3            0x3           0x3              0x3               0x3               0x3
// 0x6806     RFE_RAM          TDCCAL1               [7:0]      SUB                   0x00           0x00          0x00             0x00              0x00              0x00
// 0x6808     RFE_RAM          TDCCAL2               [5:0]      AVG                   0x10           0x10          0x10             0x10              0x10              0x10
// 0x680A     RFE_RAM          TDCPLL                [10:8]     STOP                  0x4            0x4           0x4              0x4               0x4               0x4
// 0x680C     RFE_RAM          K1LSB                 [15:0]     VAL                   0x569B         0x569B        0x569B           0x569B            0x569B            0x569B
// 0x680E     RFE_RAM          K1MSB                 [15:0]     VAL                   0x010A         0x010A        0x010A           0x010A            0x010A            0x010A
// 0x6810     RFE_RAM          K2BL                  [15]       HPM                   0x0            0x0           0x0              0x0               0x0               0x0
// 0x6810     RFE_RAM          K2BL                  [14:0]     VAL                   0x0383         0x0383        0x0383           0x0383            0x0383            0x0383
// 0x6812     RFE_RAM          K2AL                  [15]       HPM                   0x1            0x1           0x1              0x1               0x1               0x1
// 0x6812     RFE_RAM          K2AL                  [14:0]     VAL                   0x056B         0x056B        0x056B           0x056B            0x056B            0x056B
// 0x6814     RFE_RAM          K3BL                  [15:0]     VAL                   0x2659         0x2659        0x2659           0x2659            0x2659            0x2659
// 0x6816     RFE_RAM          K3AL                  [15:0]     VAL                   0x02E0         0x02E0        0x02E0           0x02E0            0x02E0            0x02E0
// 0x681E     RFE_RAM          RTRIMOFF              [3:0]      VAL                   0x0            0x0           0x0              0x0               0x0               0x0
// 0x6820     RFE_RAM          RTRIMMIN              [3:0]      VAL                   0x0            0x0           0x0              0x0               0x0               0x0
// 0x6822     RFE_RAM          DIVI                  [15]       PDET                  0x0            0x0           0x0              0x0               0x0               0x0
// 0x6822     RFE_RAM          DIVI                  [14:12]    NMIREFTRIM            0x0            0x0           0x0              0x0               0x0               0x0
// 0x6822     RFE_RAM          DIVI                  [11:9]     PMIREFTRIM            0x0            0x0           0x0              0x0               0x0               0x0
// 0x6822     RFE_RAM          DIVI                  [8:0]      RATIO                 0x104          0x104         0x104            0x104             0x104             0x104
// 0x6824     RFE_RAM          DIVF                  [15]       PDET                  0x0            0x0           0x0              0x0               0x0               0x0
// 0x6824     RFE_RAM          DIVF                  [14:12]    NMIREFTRIM            0x4            0x4           0x4              0x4               0x4               0x4
// 0x6824     RFE_RAM          DIVF                  [11:9]     PMIREFTRIM            0x4            0x4           0x4              0x4               0x4               0x4
// 0x6824     RFE_RAM          DIVF                  [8:0]      RATIO                 0x104          0x104         0x104            0x104             0x104             0x104
// 0x6826     RFE_RAM          DIVLDOI               [14:8]     VOUTTRIM              0x00           0x00          0x00             0x00              0x00              0x00
// 0x6828     RFE_RAM          DIVLDOF               [14:8]     VOUTTRIM              0x00           0x00          0x00             0x00              0x00              0x00
// 0x682A     RFE_RAM          DIVLDOIOFF            [6:0]      VAL                   0x14           0x14          0x14             0x14              0x14              0x14
// 0x682C     RFE_RAM          LDOSETTLE             [9:0]      VAL                   0x138          0x138         0x138            0x138             0x138             0x138
// 0x682E     RFE_RAM          CHRGSETTLE            [9:0]      VAL                   0x000          0x000         0x000            0x000             0x000             0x000
// 0x6830     RFE_RAM          DIVSETTLE             [9:0]      VAL                   0x018          0x018         0x018            0x018             0x018             0x018
// 0x6832     RFE_RAM          RFLDOTX               [7:1]      TRIM                  0x00           0x00          0x00             0x00              0x00              0x00
// 0x6834     RFE_RAM          RFLDORX               [7:1]      TRIM                  0x00           0x00          0x00             0x00              0x00              0x00
// 0x6836     RFE_RAM          RFLDOOFDMTX           [7:1]      TRIM                  0x00           0x00          0x00             0x00              0x00              0x00
// 0x6838     RFE_RAM          RTRIMTX               [6:3]      TRIM                  0x0            0x0           0x0              0x0               0x0               0x0
// 0x683A     RFE_RAM          RTRIMRX               [6:3]      TRIM                  0x0            0x0           0x0              0x0               0x0               0x0
// 0x683C     RFE_RAM          DCOAMPSETTLE          [9:0]      VAL                   0x048          0x048         0x048            0x048             0x048             0x048
// 0x683E     RFE_RAM          DCOLDO0VTRIM          [7:0]      VOUTTRIM              0x00           0x00          0x00             0x00              0x00              0x00
// 0x6840     RFE_RAM          DCOLDO0VMAX           [7:0]      VOUTTRIM              0xFF           0xFF          0xFF             0xFF              0xFF              0xFF
// 0x6842     RFE_RAM          DCOOVR0INIT           [13:8]     MIDCODE               0x3F           0x3F          0x3F             0x3F              0x3F              0x3F
// 0x6842     RFE_RAM          DCOOVR0INIT           [7:4]      CRSCODE               0xF            0xF           0xF              0xF               0xF               0xF
// 0x6842     RFE_RAM          DCOOVR0INIT           [1]        MIDCTL                0x1            0x1           0x1              0x1               0x1               0x1
// 0x6842     RFE_RAM          DCOOVR0INIT           [0]        CRSCTL                0x1            0x1           0x1              0x1               0x1               0x1
// 0x6844     RFE_RAM          CALTIMEOUT            [15:0]     VAL                   0x0FFF         0x0FFF        0x0FFF           0x0FFF            0x0FFF            0x0FFF
// 0x6846     RFE_RAM          FSTXTONE              [15:0]     VAL                   0x00F0         0x00F0        0x00F0           0x00F0            0x00F0            0x00F0
// 0x6848     RFE_RAM          FSBWSETTLE            [7:0]      VAL                   0x18           0x18          0x18             0x18              0x18              0x18
// 0x684A     RFE_RAM          IFADCDLDORX           [13:8]     TRIMOUT               0x00           0x00          0x00             0x00              0x00              0x00
// 0x684A     RFE_RAM          IFADCDLDORX           [6]        ATESTOUT              0x0            0x0           0x0              0x0               0x0               0x0
// 0x684A     RFE_RAM          IFADCDLDORX           [5]        ATESTFB               0x0            0x0           0x0              0x0               0x0               0x0
// 0x684A     RFE_RAM          IFADCDLDORX           [4]        ATESTERRAMP           0x0            0x0           0x0              0x0               0x0               0x0
// 0x684A     RFE_RAM          IFADCDLDORX           [3]        ITEST                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x684A     RFE_RAM          IFADCDLDORX           [2]        BYPASS                0x0            0x0           0x0              0x0               0x0               0x0
// 0x684A     RFE_RAM          IFADCDLDORX           [1]        CLAMP                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x684C     RFE_RAM          IFADCDLDOTX           [13:8]     TRIMOUT               0x00           0x00          0x00             0x00              0x00              0x00
// 0x684C     RFE_RAM          IFADCDLDOTX           [6]        ATESTOUT              0x0            0x0           0x0              0x0               0x0               0x0
// 0x684C     RFE_RAM          IFADCDLDOTX           [5]        ATESTFB               0x0            0x0           0x0              0x0               0x0               0x0
// 0x684C     RFE_RAM          IFADCDLDOTX           [4]        ATESTERRAMP           0x0            0x0           0x0              0x0               0x0               0x0
// 0x684C     RFE_RAM          IFADCDLDOTX           [3]        ITEST                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x684C     RFE_RAM          IFADCDLDOTX           [2]        BYPASS                0x0            0x0           0x0              0x0               0x0               0x0
// 0x684C     RFE_RAM          IFADCDLDOTX           [1]        CLAMP                 0x0            0x0           0x0              0x0               0x0               0x0
// 0x684E     RFE_RAM          DACTRIMADD            [5:0]      VAL                   0x14           0x14          0x14             0x14              0x14              0x14
// 0x8008     OCE_MAIN_RAM     SUNSTFTHR_LOW         [15:0]     VALUE                 0x0013         0x0013        0x0013           0x0013            0x0013            0x0013
// 0x800C     OCE_MAIN_RAM     SUNLTFTHR             [15:0]     VALUE                 0x012C         0x012C        0x012C           0x012C            0x012C            0x012C


#endif
