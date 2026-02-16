.. zephyr:code-sample:: cc35xx-pm
   :name: CC35xx power management
   :relevant-api: subsys_pm_device

   Demonstrate CC35xx standby and peripheral resume.

Overview
********

This sample periodically exercises UART console output, an SPI transfer, and a
TMP1075 temperature read over I2C before entering standby through Zephyr power
management. After wakeup, the same peripherals are used again to verify that the
drivers resume correctly.

The red LED is enabled while the sample is active and disabled before the sleep
window.

Building and Running
********************

Build and flash the sample for the LP-EM-CC35x1 board:

.. zephyr-app-commands::
   :zephyr-app: samples/boards/ti/cc35xx/pm
   :board: lp_em_cc35x1
   :goals: build flash
   :compact:

Expected output includes repeated cycles similar to:

.. code-block:: console

   pm sample: boot
   pm sample: tick 0 (pre-sleep)
   pm sample: spi rc=0
   pm sample: tmp1075 27.000000 C
   pm sample: sleep enter
   pm sample: sleep exit
