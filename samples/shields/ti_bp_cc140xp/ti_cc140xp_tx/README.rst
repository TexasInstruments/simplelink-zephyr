.. _ti_cc140xp_tx_sample:

TI CC140xP Transceiver TX Sample
################################

Overview
********

This sample demonstrates the TI CC140xP transceiver driver TX functionality. The driver
communicates with the transceiver device using a 3-wire SPI bus with
manual chip select control and an additional interrupt pin (TRX_HOST_SPI_INT).
The sample transmits RF packets at regular intervals.

Building and Running
********************

This sample can be built for any board that has SPI and GPIO support. On
the LP_EM_CC2745R10_Q1, the ti_cc140xp device node is provided by the
:ref:`ti_bp_cc140xp` shield rather than a per-sample overlay:

.. code-block:: console

   west build -b lp_em_cc2745r10_q1/cc2745r10_q1 --shield ti_bp_cc140xp samples/shields/ti_bp_cc140xp/ti_cc140xp_tx
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
   [00:00:00.000,000] <inf> main: TI CC140xP Transceiver Sample Application
   [00:00:00.000,000] <inf> main: TI CC140xP device is ready
   [00:00:00.000,000] <inf> main: Driver initialized successfully - waiting for API implementation
   [00:00:05.000,000] <inf> main: TI CC140xP driver is running...
