.. _ti_cc140xp_dual_protocol_rx_sample:

TI CC140xP Dual Protocol RX Sample
###################################

Overview
********

This sample demonstrates the TI CC140xP transceiver driver's dual-protocol
receive functionality. The driver communicates with the transceiver device
using a 3-wire SPI bus with manual chip select control and an additional
interrupt pin (TRX_HOST_SPI_INT).

The sample issues a single ``TRX_RadioCommand_DualReceive`` command that
listens simultaneously on two frequencies with two different modems:

  - SUN OQPSK @ 868 MHz
  - 2-GFSK 50 kbps @ 869 MHz

Each received packet is decoded according to its modem mask and logged with
its length and RSSI. If both LED0 and LED1 are present, LED0 toggles on each
OQPSK packet and LED1 toggles on each FSK packet. If only LED0 is present,
it blinks once (100 ms) for an OQPSK packet or twice (100 ms on, 150 ms off,
100 ms on) for an FSK packet, so the two protocols remain distinguishable on
a single LED.

This sample pairs with :ref:`ti_cc140xp_dual_protocol_tx_sample` running on a
second board.

Building and Running
********************

This sample can be built for any board that has SPI and GPIO support. On
the LP_EM_CC2745R10_Q1, the ti_cc140xp device node is provided by the
:ref:`ti_bp_cc140xp` shield rather than a per-sample overlay:

.. code-block:: console

   west build -b lp_em_cc2745r10_q1/cc2745r10_q1 --shield ti_bp_cc140xp samples/shields/ti_bp_cc140xp/ti_cc140xp_dual_protocol_rx
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
   [00:00:00.000,000] <inf> dual_protocol_rx: TI CC140xP Dual Protocol RX Sample
   [00:00:00.000,000] <inf> dual_protocol_rx: Listening for OQPSK @ 868 MHz and 2GFSK @ 869 MHz
   [00:00:00.000,000] <inf> dual_protocol_rx: TRX Host opened
   [00:00:00.100,000] <inf> dual_protocol_rx: Device config stored (PropFSKPeakAGC)
   [00:00:00.200,000] <inf> dual_protocol_rx: NV erased
   [00:00:00.300,000] <inf> dual_protocol_rx: FE config stored
   [00:00:00.400,000] <inf> dual_protocol_rx: PHY config stored
   [00:00:00.400,000] <inf> dual_protocol_rx: Listening for dual protocol packets (OQPSK 868 MHz / FSK 869 MHz)...
   [00:00:02.000,000] <inf> dual_protocol_rx: RX #1 OQPSK 868 MHz: 20 bytes, RSSI -42 dBm
   [00:00:04.000,000] <inf> dual_protocol_rx: RX #2 FSK 869 MHz: 20 bytes, RSSI -45 dBm
