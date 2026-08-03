.. _ti_cc140xp_mdr_tx_cca_sample:

TI CC140xP Wi-SUN MDR TX with CCA Sample
##########################################

Overview
********

This sample demonstrates the TI CC140xP transceiver driver's Wi-SUN
Multi-Data-Rate (MDR) transmit functionality with Clear Channel Assessment
(CCA) added before each transmission. The driver communicates with the
transceiver device using a 3-wire SPI bus with manual chip select control
and an additional interrupt pin (TRX_HOST_SPI_INT).

This is a variant of :ref:`ti_cc140xp_mdr_tx_sample` that inserts a Carrier
Sense command ahead of both the header and packet transmissions, forming a
four-command chain: CS(header) -> TX(header) -> CS(packet) -> TX(packet).
Each CS command measures energy against a -80 dBm RSSI threshold; if the
channel is busy, the corresponding TX is skipped and the chain terminates
early with a logged warning. The header uses FSK Mode 2B (channel 10,
ChannelPlanID 22, ~924.9 MHz); the packet uses OFDM Option 2 at MCS5
(16-QAM, 1/2 FEC).

On a button press (BTN1/sw0), the sample attempts 100 header+packet pairs
back to back, 100 ms apart, each carrying a 10-byte payload with an
incrementing sequence number. If no button is present in the device tree,
the sample instead attempts a burst of 100 pairs automatically every 5
seconds. The LED toggles after every pair attempt (sent or skipped on
channel-busy).

This sample pairs with :ref:`ti_cc140xp_mdr_rx_sample` running on a second
board.

Building and Running
********************

This sample can be built for any board that has SPI and GPIO support. On
the LP_EM_CC2745R10_Q1, the ti_cc140xp device node is provided by the
:ref:`ti_bp_cc140xp` shield rather than a per-sample overlay:

.. code-block:: console

   west build -b lp_em_cc2745r10_q1/cc2745r10_q1 --shield ti_bp_cc140xp samples/shields/ti_bp_cc140xp/ti_cc140xp_mdr_tx_cca
   west flash

The shield wires the following pin assignments on the LP_EM_CC2745R10_Q1:

  - SPI0 SCLK: DIO3
  - SPI0 POCI: DIO4
  - SPI0 PICO: DIO5
  - CS:        DIO7
  - RESET:     DIO16
  - INT:       DIO28
  - BTN1 (sw0): starts a burst of 100 MDR CCA pairs

For other boards, provide your own overlay defining the ti_cc140xp device
node - see samples/shields/ti_bp_cc140xp/new_board.overlay.example for a starting
point.

Sample Output
=============

.. code-block:: console

   *** Booting Zephyr OS build v3.7.0 ***
   [00:00:00.000,000] <inf> main: TI CC140xP Wi-SUN MDR TX with CCA Sample
   [00:00:00.000,000] <inf> main: TRX Host opened
   [00:00:00.100,000] <inf> main: NV erased
   [00:00:00.200,000] <inf> main: FE config stored
   [00:00:00.300,000] <inf> main: PHY config stored
   [00:00:00.350,000] <inf> main: TRX firmware v1.2
   [00:00:00.400,000] <inf> main: Header: ChannelPlanID=22 ch=10 freq=924900 kHz
   [00:00:00.400,000] <inf> main: Packet: ChannelPlanID=22 freq=925200 kHz (delta=300 kHz)
   [00:00:00.500,000] <inf> main: Delta table stored
   [00:00:00.500,000] <inf> main: Ready -- press BTN1 to send 100 MDR CCA pairs
   [00:00:02.000,000] <inf> main: TX MDR CCA pair #1 sent
   [00:00:02.100,000] <wrn> main: CS slot 2: channel busy, skipping TX
   [00:00:02.100,000] <inf> main: TX MDR CCA pair #2 sent
   ...
   [00:00:12.000,000] <inf> main: Burst complete: 100 MDR CCA pairs sent
