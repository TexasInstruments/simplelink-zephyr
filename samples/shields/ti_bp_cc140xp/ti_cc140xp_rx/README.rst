.. _ti_cc140xp_rx_sample:

TI CC140xP Transceiver RX Sample
################################

Overview
********

This sample demonstrates the TI CC140xP transceiver driver RX functionality
using the Wi-SUN FSK Mode 2B PHY, as opposed to
:ref:`ti_cc140xp_genfsk_rx_sample`, which uses the generic 2-GFSK PHY. The
driver communicates with the transceiver device using a 3-wire SPI bus with
manual chip select control and an additional interrupt pin (TRX_HOST_SPI_INT).
The sample continuously listens for and receives RF packets.

Building and Running
********************

This sample can be built for any board that has SPI and GPIO support. On
the LP_EM_CC2745R10_Q1, the ti_cc140xp device node is provided by the
:ref:`ti_bp_cc140xp` shield rather than a per-sample overlay:

.. code-block:: console

   west build -b lp_em_cc2745r10_q1/cc2745r10_q1 --shield ti_bp_cc140xp samples/shields/ti_bp_cc140xp/ti_cc140xp_rx
   west flash

The shield wires the following pin assignments on the LP_EM_CC2745R10_Q1:

  - SPI0 SCLK: DIO3
  - SPI0 POCI: DIO4
  - SPI0 PICO: DIO5
  - CS:        DIO7
  - RESET:     DIO16
  - INT:       DIO28

For other boards, see samples/shields/ti_bp_cc140xp/new_board.overlay.example for
a starting point.

Sample Output
=============

.. code-block:: console

   *** Booting Zephyr OS build v3.7.0 ***
   [00:00:00.000,000] <inf> ti_cc140xp: TI CC140xP transceiver initialized
   [00:00:00.000,000] <inf> main: TI CC140xP RX Sample Application
   [00:00:00.000,000] <inf> main: TI CC140xP device is ready
   [00:00:00.000,000] <inf> main: TRX Host opened successfully
   [00:00:00.000,000] <inf> main: Configuration loaded successfully
   [00:00:00.000,000] <inf> main: Starting RX operation...
   [00:00:00.000,000] <inf> main: RX command started - listening for packets...
   [00:00:05.000,000] <inf> main: Total packets received: 0
   [00:00:10.000,000] <inf> main: RX packet #1 - RSSI: -45 dBm
   [00:00:10.000,000] <inf> main: Total packets received: 1
