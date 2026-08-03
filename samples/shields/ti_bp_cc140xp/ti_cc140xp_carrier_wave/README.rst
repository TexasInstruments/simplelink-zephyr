.. _ti_cc140xp_carrier_wave_sample:

TI CC140xP RF Carrier Wave Sample
#################################

Overview
********

This sample demonstrates the TI CC140xP transceiver driver's TX test mode
functionality. The driver communicates with the transceiver device using a
3-wire SPI bus with manual chip select control and an additional interrupt
pin (TRX_HOST_SPI_INT).

After configuring the device, the sample starts a continuous TX test command
and leaves it running indefinitely.
The test mode is set to ``TRX_TxTestMode_Unmodulated`` by default — a pure carrier
wave (CW) at the center frequency, no data. The driver's TX test command also
supports ``TRX_TxTestMode_Modulated`` (continuous stream of the
``TX_TEST_PATTERN`` byte) and ``TRX_TxTestMode_PN9`` (PRBS-9 pseudo-random
sequence, for BER testing) — edit the ``mode`` field in ``src/main.c`` to try
them.

The sample transmits at 920.6 MHz using the SUN FSK Mode 2B PHY.

Building and Running
********************

This sample can be built for any board that has SPI and GPIO support. On
the LP_EM_CC2745R10_Q1, the ti_cc140xp device node is provided by the
:ref:`ti_bp_cc140xp` shield rather than a per-sample overlay:

.. code-block:: console

   west build -b lp_em_cc2745r10_q1/cc2745r10_q1 --shield ti_bp_cc140xp samples/shields/ti_bp_cc140xp/ti_cc140xp_carrier_wave
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
   [00:00:00.000,000] <inf> main: TI CC140xP RF Carrier Wave Sample
   [00:00:00.000,000] <inf> main: TRX Host opened
   [00:00:00.100,000] <inf> main: NV erased
   [00:00:00.200,000] <inf> main: FE config stored
   [00:00:00.300,000] <inf> main: PHY config stored
   [00:00:00.300,000] <inf> main: Mode: Unmodulated CW at 920600 kHz
   [00:00:00.400,000] <inf> main: Transmitting -- press RESET to stop
