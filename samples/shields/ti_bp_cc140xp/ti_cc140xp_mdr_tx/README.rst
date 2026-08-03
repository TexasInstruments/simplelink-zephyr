.. _ti_cc140xp_mdr_tx_sample:

TI CC140xP Wi-SUN MDR TX Sample
################################

Overview
********

This sample demonstrates the TI CC140xP transceiver driver's Wi-SUN
Multi-Data-Rate (MDR) transmit functionality. The driver communicates with
the transceiver device using a 3-wire SPI bus with manual chip select
control and an additional interrupt pin (TRX_HOST_SPI_INT).

Each MDR transmission is a chained pair of commands: a short FSK Mode 2B
"header" (channel 10, ChannelPlanID 22, ~924.9 MHz) that announces the
modulation and rate of the packet to follow, and the packet itself sent on
an OFDM Option 4 PHY at MCS4 (QPSK, 3/4 FEC, ~200 kbps) — the driver chains
the packet transmission automatically 1 ms after the header completes, and
retunes using a JP frequency delta table so only the header needs full
calibration.

On a button press (BTN1/sw0), the sample transmits 100 header+packet pairs
back to back, 100 ms apart, each carrying a 10-byte payload with an
incrementing sequence number. If no button is present in the device tree,
the sample instead transmits a burst of 100 pairs automatically every 5
seconds. The LED toggles after every pair sent.

This sample pairs with :ref:`ti_cc140xp_mdr_rx_sample` running on a second
board. See also :ref:`ti_cc140xp_mdr_tx_cca_sample` for a variant that adds a
Clear Channel Assessment before each transmission.

Building and Running
********************

This sample can be built for any board that has SPI and GPIO support. On
the LP_EM_CC2745R10_Q1, the ti_cc140xp device node is provided by the
:ref:`ti_bp_cc140xp` shield rather than a per-sample overlay:

.. code-block:: console

   west build -b lp_em_cc2745r10_q1/cc2745r10_q1 --shield ti_bp_cc140xp samples/shields/ti_bp_cc140xp/ti_cc140xp_mdr_tx
   west flash

The shield wires the following pin assignments on the LP_EM_CC2745R10_Q1:

  - SPI0 SCLK: DIO3
  - SPI0 POCI: DIO4
  - SPI0 PICO: DIO5
  - CS:        DIO7
  - RESET:     DIO16
  - INT:       DIO28
  - BTN1 (sw0): starts a burst of 100 MDR pairs

For other boards, provide your own overlay defining the ti_cc140xp device
node - see samples/shields/ti_bp_cc140xp/new_board.overlay.example for a starting
point.

Sample Output
=============

.. code-block:: console

   *** Booting Zephyr OS build v3.7.0 ***
   [00:00:00.000,000] <inf> main: TI CC140xP Wi-SUN MDR TX Sample
   [00:00:00.000,000] <inf> main: TRX Host opened
   [00:00:00.100,000] <inf> main: NV erased
   [00:00:00.200,000] <inf> main: FE config stored
   [00:00:00.300,000] <inf> main: PHY config stored
   [00:00:00.300,000] <inf> main: Header: ChannelPlanID=22 ch=10 freq=924900 kHz
   [00:00:00.300,000] <inf> main: Packet: ChannelPlanID=22 freq=925500 kHz (delta=600 kHz)
   [00:00:00.400,000] <inf> main: Delta table stored
   [00:00:00.400,000] <inf> main: Ready -- press BTN1 to send 100 MDR pairs
   [00:00:02.000,000] <inf> main: TX MDR pair #1 sent
   [00:00:02.100,000] <inf> main: TX MDR pair #2 sent
   ...
   [00:00:12.000,000] <inf> main: Burst complete: 100 MDR pairs sent
