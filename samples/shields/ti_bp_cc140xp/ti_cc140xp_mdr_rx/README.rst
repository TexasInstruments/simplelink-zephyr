.. _ti_cc140xp_mdr_rx_sample:

TI CC140xP Wi-SUN MDR RX Sample
################################

Overview
********

This sample demonstrates the TI CC140xP transceiver driver's Wi-SUN
Multi-Data-Rate (MDR) receive functionality. The driver communicates with
the transceiver device using a 3-wire SPI bus with manual chip select
control and an additional interrupt pin (TRX_HOST_SPI_INT).

Wi-SUN MDR sends every packet as two back-to-back transmissions: a short
FSK Mode 2B "header" that announces the modulation and rate of the packet
that follows, and the packet itself at that (typically much faster) rate.
This sample listens on the FSK Mode 2B header PHY at channel 10
(ChannelPlanID 22, ~924.9 MHz) with MDR enabled; the driver decodes the
header, retunes to the announced PHY using a JP frequency delta table, and
delivers the packet payload once received.

This sample pairs with :ref:`ti_cc140xp_mdr_tx_sample` (or
:ref:`ti_cc140xp_mdr_tx_cca_sample`) running on a second board.

Received packets are decoded (OFDM MCS rate or FSK, as announced by the
header) and logged with their length and RSSI. The LED toggles on every
packet received.

Building and Running
********************

This sample can be built for any board that has SPI and GPIO support. On
the LP_EM_CC2745R10_Q1, the ti_cc140xp device node is provided by the
:ref:`ti_bp_cc140xp` shield rather than a per-sample overlay:

.. code-block:: console

   west build -b lp_em_cc2745r10_q1/cc2745r10_q1 --shield ti_bp_cc140xp samples/shields/ti_bp_cc140xp/ti_cc140xp_mdr_rx
   west flash

The shield wires the following pin assignments on the LP_EM_CC2745R10_Q1:

  - SPI0 SCLK: DIO3
  - SPI0 POCI: DIO4
  - SPI0 PICO: DIO5
  - CS:        DIO7
  - RESET:     DIO16
  - INT:       DIO28

For other boards, provide your own overlay defining the ti_cc140xp device
node - see samples/shields/ti_bp_cc140xp/new_board.overlay.example for a starting
point.

Sample Output
=============

.. code-block:: console

   *** Booting Zephyr OS build v3.7.0 ***
   [00:00:00.000,000] <inf> main: TI CC140xP Wi-SUN MDR RX Sample
   [00:00:00.000,000] <inf> main: TRX Host opened
   [00:00:00.100,000] <inf> main: NV erased
   [00:00:00.200,000] <inf> main: FE config stored
   [00:00:00.300,000] <inf> main: PHY config stored
   [00:00:00.300,000] <inf> main: Header: ChannelPlanID=22 ch=10 freq=924900 kHz
   [00:00:00.400,000] <inf> main: Delta table stored
   [00:00:00.400,000] <inf> main: Listening for Wi-SUN MDR packets on ch 10 (924900 kHz)...
   [00:00:01.500,000] <inf> main: RX MDR #1 (10 bytes, RSSI: -38 dBm, OFDM MCS4)
