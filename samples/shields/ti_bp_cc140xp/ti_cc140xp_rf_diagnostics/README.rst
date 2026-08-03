.. _ti_cc140xp_rf_diagnostics_sample:

TI CC140xP RF Diagnostics Sample
##################################

Overview
********

This sample is an interactive AT-command RF diagnostics/certification tool
for the TI CC140xP transceiver, ported from TI's SimpleLink RF Diagnostics
application. Unlike the other ``ti_cc140xp_*`` samples, it does not run a
fixed TX or RX loop — instead it starts an AT-command interpreter over the
console UART and waits for commands, letting a host PC (terminal or GUI
tool) select PHYs, frequencies, power levels, and test modes at runtime.

The driver communicates with the transceiver device using a 3-wire SPI bus
with manual chip select control and an additional interrupt pin
(TRX_HOST_SPI_INT), same as the other samples.

.. code-block:: text

   [Laptop or similar] --UART--> [rfDiagnostics device] --SPI--> [CC140XP]

Building and Running
********************

This sample can be built for any board that has SPI, GPIO, and a console
UART. On the LP_EM_CC2745R10_Q1, the ti_cc140xp device node is provided by
the :ref:`ti_bp_cc140xp` shield rather than a per-sample overlay:

.. code-block:: console

   west build -b lp_em_cc2745r10_q1/cc2745r10_q1 --shield ti_bp_cc140xp samples/shields/ti_bp_cc140xp/ti_cc140xp_rf_diagnostics
   west flash

The shield wires the following pin assignments on the LP_EM_CC2745R10_Q1:

  - SPI0 SCLK: DIO3
  - SPI0 POCI: DIO4
  - SPI0 PICO: DIO5
  - CS:        DIO7
  - RESET:     DIO16
  - INT:       DIO28

The console UART used for the AT command interface is provided by the
board itself (not the shield):

  - UART0 TXD: DIO1
  - UART0 RXD: DIO2

Connect a serial terminal to the board's console UART (115200 8N1 by
default) to send AT commands.

Using the AT Command Interface
===============================

Every command starts with the literal characters ``AT``, followed by a
control or parameter command and a carriage return. Commands are
case-insensitive. On success the sample replies ``OK``; on failure it
replies with an error code.

Control Commands (radio init/reset):

=======  ============================================================
Command  Description
=======  ============================================================
``I``    Initialize the radio and PHY, e.g. ``ATI0 0`` inits radio 0
         with PHY index 0
``RS``   Reset the device (``sys_reboot``)
``EM``   Enable Multi-Data-Rate (MDR) mode for a radio/region
=======  ============================================================

Parameter Commands (read with ``?``, write with ``=``):

=======  ============================================================
Command  Description
=======  ============================================================
``PP``   PER test packet count
``PL``   PER test packet length
``TM``   Test mode select — see table below
``FR``   Frequency
``PW``   TX power
``CR``   Current RSSI (read-only)
``AR``   Average RSSI (read-only)
``LR``   Minimum ("low") RSSI (read-only)
``MR``   Maximum RSSI (read-only)
``PN``   Supported PHY names (read-only)
``AE``   AT command echo on/off
``RV``   Radio firmware version (read-only)
=======  ============================================================

Test Modes Selectable via ``ATTM=<n>``:

==  ========================
 n  Mode
==  ========================
 0  Exit test mode
 1  Carrier wave (CW)
 2  Modulated TX
 3  Continuous RX
 4  PER TX
 5  PER RX
 6  MDR TX
 7  MDR RX
 8  MDR TX with CCA
 9  CCA TX
==  ========================

Example Session (two boards, radio 2 on PHY 0, Wi-SUN Mode #2b at 920.9 MHz)
==============================================================================

This example uses two boards side by side: one runs a PER TX test to send
100 packets of 200-byte payload, and the other runs the matching PER RX
test to receive and score them.

TX Board
--------

.. code-block:: console

   AT+I2 0
   OK
   ATPFR2=920900000
   OK
   ATPPP2=100
   OK
   ATPPL2=200
   OK
   ATPTM2=4
   OK

RX Board
--------

.. code-block:: console

   AT+I2 0
   OK
   ATPFR2=920900000
   OK
   ATPTM2=5
   OK
   ATPTM2=0
   OK

Sample Output
=============

TX Board
--------

.. code-block:: console

   *** Booting Zephyr OS build v3.7.0 ***
   RF Diagnostics Example
   AT+I2 0
   Init State
   OK

   ATPFR2=920900000
   OK

   ATPPP2=100
   OK

   ATPPL2=200
   OK

   ATPTM2=4
   Sending packets....
   Packets Transmitted: 100
   OK

RX Board
--------

.. code-block:: console

   *** Booting Zephyr OS build v3.7.0 ***
   RF Diagnostics Example
   AT+I2 0
   Init State
   OK

   ATPFR2=920900000
   OK

   ATPTM2=5
   RX in Progress....
   Exit by setting Test Mode = 0 (ATPTM=0)
   OK

   ATPTM2=0
   Packets Received: 100
   CRC Ok:           100
   Sync Ok:          100
   PER:              0%
   Average RSSI: 0 dBm
   Max RSSI:     2 dBm
   Min RSSI:     -6 dBm
   PHY RX'd: PHY 0: WiSUN Mode #2b, PhySettings_TEST_STUDIO_COMPL
   OK