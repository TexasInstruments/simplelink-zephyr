.. _ti_cc140xp_dual_protocol_tx_sample:

TI CC140xP Dual Protocol TX Sample
###################################

Overview
********

This sample demonstrates the TI CC140xP transceiver driver's dual-protocol
transmit functionality. The driver communicates with the transceiver device
using a 3-wire SPI bus with manual chip select control and an additional
interrupt pin (TRX_HOST_SPI_INT).

The sample alternates, once every 2 seconds, between two TX commands:

  - SUN OQPSK @ 868 MHz
  - 2-GFSK 50 kbps @ 869 MHz

Each packet carries a 20-byte payload of ``0xAA`` bytes. If both LED0 and
LED1 are present, LED0 toggles after each OQPSK transmission and LED1
toggles after each FSK transmission. If only LED0 is present, it blinks
once for an OQPSK packet or twice for an FSK packet, matching the blink
pattern used by :ref:`ti_cc140xp_dual_protocol_rx_sample`.

This sample pairs with :ref:`ti_cc140xp_dual_protocol_rx_sample` running on a
second board.

Building and Running
********************

This sample can be built for any board that has SPI and GPIO support. On
the LP_EM_CC2745R10_Q1, the ti_cc140xp device node is provided by the
:ref:`ti_bp_cc140xp` shield rather than a per-sample overlay:

.. code-block:: console

   west build -b lp_em_cc2745r10_q1/cc2745r10_q1 --shield ti_bp_cc140xp samples/shields/ti_bp_cc140xp/ti_cc140xp_dual_protocol_tx
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
   [00:00:00.000,000] <inf> dual_protocol_tx: TI CC140xP Dual Protocol Packet TX Sample
   [00:00:00.000,000] <inf> dual_protocol_tx: Alternates between OQPSK @ 868 MHz and 2GFSK @ 869 MHz
   [00:00:00.000,000] <inf> dual_protocol_tx: TI CC140xP device is ready
   [00:00:00.000,000] <inf> dual_protocol_tx: TRX Host driver opened successfully
   [00:00:00.100,000] <inf> dual_protocol_tx: TRX device configured successfully (PropFSKPeakAGC mode)
   [00:00:00.200,000] <inf> dual_protocol_tx: NV erased successfully
   [00:00:00.300,000] <inf> dual_protocol_tx: FE configuration persisted successfully
   [00:00:00.400,000] <inf> dual_protocol_tx: PHY configuration persisted successfully
   [00:00:00.400,000] <inf> dual_protocol_tx: OQPSK TX command stored successfully
   [00:00:00.400,000] <inf> dual_protocol_tx: FSK TX command stored successfully
   [00:00:00.500,000] <inf> dual_protocol_tx: OQPSK TX packet sent successfully (868 MHz)
   [00:00:02.500,000] <inf> dual_protocol_tx: FSK TX packet sent successfully (869 MHz)
