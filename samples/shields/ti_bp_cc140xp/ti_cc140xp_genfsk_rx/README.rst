.. _ti_cc140xp_genfsk_rx_sample:

TI CC140xP GenFSK RX Sample
############################

Overview
********

This sample demonstrates the TI CC140xP transceiver driver RX functionality
using the generic 2-GFSK 50 kbps PHY. The driver communicates with the
transceiver device using a 3-wire SPI bus with manual chip select control
and an additional interrupt pin (TRX_HOST_SPI_INT).

The sample continuously listens for packets at 868 MHz. Each received
packet uses a 1-byte length header followed by the payload, RSSI, and a
timestamp. On every packet received, the LED toggles and the packet count,
length, and RSSI are logged.

This sample pairs with :ref:`ti_cc140xp_genfsk_tx_sample` running on a
second board.

Building and Running
********************

This sample can be built for any board that has SPI and GPIO support. On
the LP_EM_CC2745R10_Q1, the ti_cc140xp device node is provided by the
:ref:`ti_bp_cc140xp` shield rather than a per-sample overlay:

.. code-block:: console

   west build -b lp_em_cc2745r10_q1/cc2745r10_q1 --shield ti_bp_cc140xp samples/shields/ti_bp_cc140xp/ti_cc140xp_genfsk_rx
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
   [00:00:00.000,000] <inf> genfsk_rx: TI CC140xP RF Packet RX GenFSK Sample
   [00:00:00.000,000] <inf> genfsk_rx: TI CC140xP device is ready
   [00:00:00.100,000] <inf> genfsk_rx: TRX device configured successfully
   [00:00:00.200,000] <inf> genfsk_rx: NV erased successfully
   [00:00:00.300,000] <inf> genfsk_rx: FE configuration persisted successfully
   [00:00:00.400,000] <inf> genfsk_rx: PHY configuration persisted successfully
   [00:00:00.400,000] <inf> genfsk_rx: Listening on 868000 kHz
   [00:00:01.000,000] <inf> genfsk_rx: RX packet #1 (100 bytes, RSSI: -40 dBm)
