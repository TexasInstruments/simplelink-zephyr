.. _ti_cc140xp_uart_bridge_sample:

TI CC140xP UART Bridge
######################

Overview
********

This sample implements a wireless UART bridge using the TI CC140xP BoosterPack
shield on the CC2745 LaunchPad (LP-EM-CC2745R10-Q1). It bridges data between
a UART interface (115200 8N1) and an RF link (IEEE 802.15.4 / Proprietary FSK
at 915 MHz).

The application uses a multi-threaded architecture with three threads
communicating via k_fifo queues for thread-safe data passage.

Requirements
************

* CC2745 LaunchPad (LP-EM-CC2745R10-Q1)
* CC140xP BoosterPack (TI-BP-CC140xP)
* Zephyr RTOS v3.7+

Hardware Connections
********************

The CC140xP BoosterPack connects to the CC2745 LaunchPad via the BoosterPack
headers:

* SPI: boosterpack_spi (8 MHz)
* Reset GPIO: BoosterPack header pin 36
* Interrupt GPIO: BoosterPack header pin 10
* POCI GPIO: BoosterPack header pin 14

LEDs (on CC2745 LaunchPad):
* Green LED (DIO12, led0 alias) - Toggles on RF TX
* Red LED (DIO16, led1 alias) - Toggles on RF RX

UART:
* uart0 @ 115200 8N1 (default console pins)

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/shields/ti_bp_cc140xp/ti_cc140xp_uart_bridge
   :board: lp_em_cc2745r10_q1/cc2745r10_q1
   :shield: ti_bp_cc140xp
   :goals: build flash
   :compact:

Sample Output
=============

.. code-block:: console

   *** Booting Zephyr OS build v3.7.0 ***
   [00:00:00.000,000] <inf> ti_cc140xp_uart_bridge: ============================================
   [00:00:00.000,000] <inf> ti_cc140xp_uart_bridge: Wireless UART Bridge Starting
   [00:00:00.000,000] <inf> ti_cc140xp_uart_bridge: CC2745 + CC140xP BoosterPack
   [00:00:00.000,000] <inf> ti_cc140xp_uart_bridge: UART: 115200 8N1 <-> RF: 915 MHz FSK
   [00:00:00.000,000] <inf> ti_cc140xp_uart_bridge: ============================================
   [00:00:00.000,000] <inf> ti_cc140xp_uart_bridge: LED0 (Green) initialized on GPIO_0 pin 12
   [00:00:00.000,000] <inf> ti_cc140xp_uart_bridge: LED1 (Red) initialized on GPIO_0 pin 16
   [00:00:00.000,000] <inf> ti_cc140xp_uart_bridge: UART RX interrupt enabled
   [00:00:00.100,000] <inf> ti_cc140xp_uart_bridge: Initializing TI CC140xP TRX...
   [00:00:00.200,000] <inf> ti_cc140xp_uart_bridge: RF initialized: RX listening on 915000 kHz, TX ready
   [00:00:00.200,000] <inf> ti_cc140xp_uart_bridge: All subsystems initialized - starting threads
   [00:00:00.200,000] <inf> ti_cc140xp_uart_bridge: UART RX thread started
   [00:00:00.200,000] <inf> ti_cc140xp_uart_bridge: UART TX thread started
   [00:00:00.200,000] <inf> ti_cc140xp_uart_bridge: RF thread started (event processing)
   [00:00:00.200,000] <inf> ti_cc140xp_uart_bridge: All threads started - bridge operational

Architecture
************

Thread Design
=============

The application uses three threads communicating via k_fifo queues:

1. **UART RX Thread** (Priority 5)
   - Reads UART data via interrupt-driven ring buffer
   - Packages data when buffer reaches 255 bytes OR 20ms quiet timeout
   - Places packets into rf_tx_fifo

2. **RF Thread** (Priority 5)
   - Manages TI CC140xP TRX Host driver
   - RX callback extracts payload and places into uart_tx_fifo
   - Toggles Red LED on RX
   - Waits on semaphore for TX completion events

3. **UART TX Thread** (Priority 5)
   - Drains uart_tx_fifo and writes to UART FIFO
   - Handles partial writes with k_yield()

Data Flow
=========

.. code-block:: text

   UART RX (interrupt) → Ring Buffer → Timeout/Size Check
                                                     ↓
                                             k_fifo (rf_tx_fifo)
                                                     ↓
   UART TX Thread ← k_fifo (uart_tx_fifo) ← RF RX Callback
                                                     ↓
                                             CC140xP Radio (SPI)
                                                     ↓
   RF TX Thread → store_stream + submit_cmd → TX Callback → Green LED

RF Configuration
================

* Frequency: 915 MHz (915000 kHz)
* Modulation: FSK (TRX_RadioCommand_Modem_FSK)
* PA: High Power (TRX_TxPa_High)
* TX Power: +10 dBm
* RX Mode: Continuous (repeat=true, stream_early=true)
* Max Packet Payload: 255 bytes
* UART Quiet Timeout: 20ms

LED Behavior
============

* **Green LED (led0)**: Toggles on each RF TX packet
* **Red LED (led1)**: Toggles on each RF RX packet

Fallback (single LED only):
* **RX**: Double blink (50ms on, 50ms off, 50ms on, 50ms off)

Testing
*******

1. Connect two CC2745 + CC140xP setups
2. Open terminal on each UART (115200 8N1)
3. Type on one terminal - text appears on the other
4. Observe LED activity:
   - Sender: Green LED toggles
   - Receiver: Red LED toggles

Statistics
==========

Every 30 seconds, the main thread logs statistics:

.. code-block:: console

   [00:00:30.000,000] <inf> ti_cc140xp_uart_bridge: Stats: UART RX=1024 bytes, RF TX=4 pkts, RF RX=4 pkts, UART TX=1024 bytes

Customization
*************

Modify these constants in ``src/main.c`` to customize behavior:

* ``MAX_RF_PAYLOAD`` - Maximum RF packet payload (default 255)
* ``UART_QUIET_TIMEOUT_MS`` - Quiet timeout for packet assembly (default 20ms)
* ``RF_FREQUENCY_KHZ`` - RF frequency in kHz (default 915000)
* ``DEFAULT_TX_POWER_DBM`` - TX power in dBm (default 10)
* ``UART_BUF_SIZE`` - UART RX ring buffer size (default 512)

References
**********

* `TI CC140xP Datasheet <https://www.ti.com/product/CC140xP>`_
* `CC2745 LaunchPad User's Guide <https://www.ti.com/tool/LP-EM-CC2745R10-Q1>`_
* `Zephyr UART API <https://docs.zephyrproject.org/latest/hardware/peripherals/uart.html>`_
* `Zephyr k_fifo API <https://docs.zephyrproject.org/latest/kernel/services/kernel_obj/fifo.html>`_