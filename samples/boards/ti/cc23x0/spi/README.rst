Overview
********

Sample code for cc23x0 SPI testing.



Requirements
************

LIS2DE12 accelerometer (STEVAL-MKI175V1) as SPI slave.

Setup LP-EM-CC2340R5 | STEVAL-MKI175V1 (pin connections):
- [1] 3V3               | [1]  VDD
- [1] 3V3               | [2]  VDDIO
- [20] GND              | [13] GND
- [22] GND              | [19] CS // Select SPI mode
- [7]  DIO18 = SPI_CLK  | [20] SPC
- [15] DIO13 = SPI_MOSI | [21] SDI
- [14] DIO12 = SPI_MISO | [22] SDO

- [18] DIO11 = SPI_CS -> Unused by LIS2DE12.

Signal can be probed with scope to ensure that level changes during SPI transfer.



Building
********

west build -p always -b lp_em_cc2340r5 samples/boards/ti/cc23x0/spi



Adding board support
********************

SPI controllers with dedicated CS pins do not need to define the cs-gpios property.
So, for the standard use case, there's nothing to do for the cc23x0 SPI controller.

For CS emulation through a GPIO line, the cs-gpios property must be defined:

.. code-block:: devicetree

   &spi0 {
      [...]
      cs-gpios = <&gpio0 24 GPIO_ACTIVE_LOW>;
   };

Signal can be probed with scope to ensure that level changes during SPI transfer.



Output
******

*** Booting Zephyr OS build b3067627d481 ***
[00:00:00.004,000] <dbg> os: k_sched_unlock: scheduler unlocked (0x20000238:0)
---------- TEST_SPI ----------
[00:00:00.016,000] <dbg> spi_cc23x0: spi_context_buffers_setup: tx_bufs 0x2000113c - rx_bufs 0x20001134 - 1
[00:00:00.027,000] <dbg> spi_cc23x0: spi_context_buffers_setup: current_tx 0x2000114c (1), current_rx 0x20001144 (1), tx buf/len 0x20001158/2, rx buf/len 0x20001154/2
[00:00:00.043,000] <dbg> spi_cc23x0: spi_cc23x0_transceive: SPI transfer completed
[00:00:00.052,000] <dbg> spi_cc23x0: spi_context_update_tx: tx buf/len 0x20001159/1
[00:00:00.061,000] <dbg> spi_cc23x0: spi_context_update_rx: rx buf/len 0x20001155/1
[00:00:00.069,000] <dbg> spi_cc23x0: spi_cc23x0_transceive: SPI transfer completed
[00:00:00.078,000] <dbg> spi_cc23x0: spi_context_update_tx: tx buf/len 0/0
[00:00:00.086,000] <dbg> spi_cc23x0: spi_context_update_rx: rx buf/len 0/0
reg = 0x0f , data = 0x33 0x33
SPI slave detected: ID = 0x33
