# TI BP-CC140xP BoosterPack Samples

This directory contains sample applications for the **TI CC140xP Transceiver** (LP-CC2745R10-Q1) using the BP-CC140xP BoosterPack shield with Zephyr RTOS.

All samples use the `ti_cc140xp` driver (`drivers/misc/ti_cc140xp/`) which communicates with the transceiver via a 3-wire SPI bus with manual chip select and an interrupt pin (TRX_HOST_SPI_INT).

## Hardware Requirements

- **Host Board**: LP_EM_CC2745R10_Q1 (or any board with SPI + GPIO)
- **Shield**: BP-CC140xP-8-FE (862-870 MHz) or BP-CC140xP-9-FE (902-928 MHz)

> **Note**: The BP-CC140xP-8-FE board is limited to 862-870 MHz due to a SAW filter. The BP-CC140xP-9-FE board is limited to 902-928 MHz.

## Sample Overview

| Sample | Description | PHY / Protocol |
|--------|-------------|----------------|
| `ti_cc140xp_tx` | Basic packet transmission at fixed intervals | Wi-SUN FSK Mode 2B (SUN PHY) |
| `ti_cc140xp_rx` | Continuous packet reception with RSSI/timestamp | Wi-SUN FSK Mode 2B (SUN PHY) |
| `ti_cc140xp_uart_bridge` | Bidirectional UART-to-RF wireless bridge | Wi-SUN FSK Mode 2B (SUN PHY) |
| `ti_cc140xp_carrier_wave` | Continuous wave / modulated TX test modes | SUN FSK (CW, PN9, modulated) |
| `ti_cc140xp_genfsk_tx` | Generic 2-GFSK 50 kbps packet transmission | Proprietary 2-GFSK |
| `ti_cc140xp_genfsk_rx` | Generic 2-GFSK 50 kbps packet reception | Proprietary 2-GFSK |
| `ti_cc140xp_mdr_tx` | Wi-SUN Multi-Data-Rate (MDR) TX with FSK header + OFDM packet | Wi-SUN FSK + OFDM (MDR) |
| `ti_cc140xp_mdr_tx_cca` | MDR TX with Clear Channel Assessment (CCA) before each phase | Wi-SUN FSK + OFDM (MDR + CCA) |
| `ti_cc140xp_mdr_rx` | Wi-SUN MDR RX with automatic PHY retune via delta tables | Wi-SUN FSK + OFDM (MDR) |
| `ti_cc140xp_dual_protocol_tx` | Alternates between OQPSK @ 868 MHz and 2-GFSK @ 869 MHz | SUN OQPSK + 2-GFSK |
| `ti_cc140xp_dual_protocol_rx` | Simultaneous dual-protocol reception (OQPSK + 2-GFSK) | SUN OQPSK + 2-GFSK |
| `ti_cc140xp_rf_diagnostics` | Interactive AT-command RF diagnostics / certification tool | Multi-PHY (Wi-SUN, GenFSK, OFDM) |
| `ti_cc140xp_ssbl_host` | Secure Serial Bootloader (SSBL) firmware update host | Raw SPI/GPIO (no TRX driver) |

## Building and Running

All samples follow the same build pattern using the `ti_bp_cc140xp` shield:

```bash
west build -b lp_em_cc2745r10_q1/cc2745r10_q1 --shield ti_bp_cc140xp samples/shields/ti_bp_cc140xp/<sample_name>
west flash
```

**Example (UART Bridge):**
```bash
west build -b lp_em_cc2745r10_q1/cc2745r10_q1 --shield ti_bp_cc140xp samples/shields/ti_bp_cc140xp/ti_cc140xp_uart_bridge
west flash
```

### Shield Pin Mapping (LP_EM_CC2745R10_Q1)

| Signal | Pin |
|--------|-----|
| SPI0 SCLK | DIO3 |
| SPI0 POCI | DIO4 |
| SPI0 PICO | DIO5 |
| CS | DIO7 |
| RESET | DIO16 |
| INT | DIO28 |

For custom boards, see `new_board.overlay.example` for a starting overlay.

## Common Features

- **Device Configuration**: All samples configure DIO2=HIGH_PA, DIO3=LOW_PA_AND_LNA (BoosterPack constraints), other DIOs unchanged
- **NV Management**: NV erase, config store/persist for FE and PHY configurations
- **Power**: 21.5 dBm TX power (21 dBm + 0.5 dB fraction) for regular commands; 22.5 dBm for extended commands
- **Wi-SUN PHY**: Uses `rcl_settings_wisun.c/h` (copied from carrier_wave) for SUN FSK/OFDM
- **GenFSK PHY**: Uses `rcl_settings_2gfsk50kbps.c/h` for generic 2-GFSK 50 kbps
- **MDR Delta Tables**: JP, EU1, NA, BZ region tables available in `src/wisun/`

## Extended Command Support

GenFSK samples (`genfsk_tx`, `genfsk_rx`) support optional extended commands via `USE_EXTENDED_COMMANDS=1`:
- Frequency in Hz (vs kHz)
- Fixed packet length
- RSSI threshold (RX only)

## SSBL Host

The `ti_cc140xp_ssbl_host` sample uses raw SPI/GPIO (not the `ti_cc140xp` driver) to program firmware into the transceiver's Secure Serial Bootloader. Requires firmware blob pre-programmed at flash offset `0x77000`.

## License

SPDX-License-Identifier: Apache-2.0