.. _ti_cc140xp_genfsk_tx_sample:

TI CC140xP GenFSK TX Sample
############################

Overview
********

This sample demonstrates the TI CC140xP transceiver driver TX functionality
using the generic 2-GFSK 50 kbps PHY. The driver communicates with the
transceiver device using a 3-wire SPI bus with manual chip select control
and an additional interrupt pin (TRX_HOST_SPI_INT).

The sample transmits a 20-byte packet at 868 MHz every 500 ms, using the
highest entry in the calibrated High-PA power table. Each packet carries a
1-byte length header, a 2-byte sequence number, and random filler bytes.
The LED toggles after each successful transmission.

This sample pairs with :ref:`ti_cc140xp_genfsk_rx_sample` running on a
second board.

Building and Running
********************

This sample can be built for any board that has SPI and GPIO support. On
the LP_EM_CC2745R10_Q1, the ti_cc140xp device node is provided by the
:ref:`ti_bp_cc140xp` shield rather than a per-sample overlay:

.. code-block:: console

   west build -b lp_em_cc2745r10_q1/cc2745r10_q1 --shield ti_bp_cc140xp samples/shields/ti_bp_cc140xp/ti_cc140xp_genfsk_tx
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
   [00:00:00.000,000] <inf> genfsk_tx: TI CC140xP RF Packet TX GenFSK Sample
   [00:00:00.000,000] <inf> genfsk_tx: TI CC140xP device is ready
   [00:00:00.100,000] <inf> genfsk_tx: NV erased successfully
   [00:00:00.200,000] <inf> genfsk_tx: TRX device configured successfully
   [00:00:00.300,000] <inf> genfsk_tx: FE configuration persisted successfully
   [00:00:00.400,000] <inf> genfsk_tx: PHY configuration persisted successfully
   [00:00:00.400,000] <inf> genfsk_tx: TX command stored - transmitting on 868000 Hz
   [00:00:00.500,000] <inf> genfsk_tx: TX packet #1 sent
   [00:00:01.000,000] <inf> genfsk_tx: TX packet #2 sent
