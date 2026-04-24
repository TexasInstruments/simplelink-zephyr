.. _lp_em_cc35x1:

TI cc35x1 Launchpad
#####################

Overview
********

The Texas Instruments cc35x1 LaunchPad |trade| (LP_EM_CC35X1) is a
development kit for the SimpleLink |trade| multi-Standard cc35x1 wireless MCU.

See the `TI cc35x1 LaunchPad Product Page`_ for details.

Hardware
********

The cc35x1 LaunchPad |trade| development kit features the cc3551e wireless MCU.
The board is equipped with two LEDs and two push buttons.

The cc35x1 wireless MCU has a 160 MHz Arm |reg| Cortex |reg|-M33 SoC and an
integrated 2.4 GHz transceiver supporting multiple protocols, including
Bluetooth |reg| Low Energy and Wi-Fi |reg| 6.

See the `TI cc35x1 Product Page`_ for additional details.

Supported Features
==================

The cc35x1 LaunchPad board configuration supports the following hardware
features:

+--------------+------------+----------------------+
| Interface    | Controller | Driver/Component     |
+==============+============+======================+
| GPIO         | on-chip    | gpio                 |
+--------------+------------+----------------------+
| DMA          | on-chip    | dma_ti_cc35xx        |
+--------------+------------+----------------------+
| UART         | on-chip    | uart_cc35xx          |
| UART+IRQ     | on-chip    | uart_cc35xx          |
| UART+IRQ+DMA | on-chip    | uart_cc35xx          |
+--------------+------------+----------------------+
| ADC          | on-chip    | adc_ti_cc35xx        |
+--------------+------------+----------------------+
| I2C MM       | on-chip    | i2c_cc35xx           |
+--------------+------------+----------------------+
| SPI          | on-chip    | spi_cc35xx           |
+--------------+------------+----------------------+
| HW CRYPTO    | on-chip    | crypto_cc35xx        |
+--------------+------------+----------------------+
| HW ENTROPY   | on-chip    | entropy_cc35xx       |
+--------------+------------+----------------------+
| HSM          | on-chip    | ti_cc35xx_hsm        |
+--------------+------------+----------------------+
| PWM          | on-chip    | pwm_cc35xx_timer     |
+--------------+------------+----------------------+
| counter      | on-chip    | counter_cc35xx_lgpt  |
+--------------+------------+----------------------+
| RTC          | on-chip    | rtc_cc35xx           |
+--------------+------------+----------------------+
| Watchdog     | on-chip    | wdt_cc35xx           |
+--------------+------------+----------------------+
| Flash        | on-chip    | soc_flash_cc35xx     |
+--------------+------------+----------------------+
| Wi-Fi        | on-chip    | ti_cc35xx_wifi       |
+--------------+------------+----------------------+
| BLE          | on-chip    | hci_ti_cc35xx        |
+--------------+------------+----------------------+
| I2S          | on-chip    | i2s_ti_cc35xx        |
+--------------+------------+----------------------+
| SDHC         | on-chip    | ti_cc35xx_sdhc       |
+--------------+------------+----------------------+


Other hardware features have not yet been enabled for this board.


Twister tests
*************

Supported tests
===============

The following Twister tests are expected to pass on the LP_EM_CC35X1:
+--------------------------------------+------------+----------------------+
| Test                                 | Level      | Component/driver     |
+======================================+============+======================+
| tests/drivers/gpio                   | default    | gpio                 |
+--------------------------------------+------------+----------------------+
| tests/drivers/uart/uart_elementary   | default    | uart_cc35xx          |
| tests/drivers/uart/uart_async_api    | default    | uart_cc35xx          |
+--------------------------------------+------------+----------------------+
| tests/drivers/dma/loop_transfer      | default    | dma_ti_cc35xx        |
+--------------------------------------+------------+----------------------+
| tests/drivers/adc/adc_api/           | default    | adc_ti_cc35xx        |
+--------------------------------------+------------+----------------------+
| tests/drivers/i2s/i2s_api            | default    | i2s_ti_cc35xx        |
+--------------------------------------+------------+----------------------+
| tests/drivers/disk/disk_access       | default    | ti_cc35xx_sdhc       |
+--------------------------------------+------------+----------------------+
| tests/drivers/disk/disk_performance  | default    | ti_cc35xx_sdhc       |
+--------------------------------------+------------+----------------------+

Twister tests execution
=======================

Basic syntax to execute twister tests
 ``west twister  --device-testing --device-serial <serial> -p lp_em_cc35x1 -T <test> <opts>``

where:
- serial: The serial console to the board e.g. ``/dev/ttyACM0``
- test: The test to execute e.g. ``zephyr/tests/drivers/gpio/``
- opts: Additional options to pass to the test e.g. ``--fixture gpio_loopback``

+------------------------+-----------------------------------------------------+
| COMPONENT              | Parameters                                          |
+========================+=====================================================+
| GPIO                   |- test: tests/drivers/gpio/basic_api                 |
|                        |- fixture: gpio_loopback                             |
+------------------------+-----------------------------------------------------+
| UART                   |- test: tests/drivers/uart/uart_elementary           |
|                        |- fixture: gpio_loopback                             |
+------------------------+-----------------------------------------------------+
| UART + DMA             |- test: tests/drivers/uart/uart_async_api            |
|                        |- fixture: gpio_loopback                             |
+------------------------+-----------------------------------------------------+
| DMA                    |- test: tests/drivers/dma/loop_transfer              |
+------------------------+-----------------------------------------------------+
| ADC                    |- test: tests/drivers/adc/adc_api                    |
+------------------------+-----------------------------------------------------+
| Watchdog               |- test: tests/drivers/watchdog/wdt_basic_reset_none/ |
+------------------------+-----------------------------------------------------+
| Flash                  |- test: tests/drivers/flash                          |
+------------------------+-----------------------------------------------------+
| I2S                    |- test: tests/drivers/i2s/i2s_api                    |
|                        |- fixture: gpio_loopback                             |
+------------------------+-----------------------------------------------------+
| SDHC                   |- test: tests/drivers/disk/disk_access               |
|                        |- test: tests/drivers/disk/disk_performance          |
+------------------------+-----------------------------------------------------+

Wi-Fi testing
*************

Build and flash the Wi-Fi sample:

.. code-block:: console

   west build -b lp_em_cc35x1 samples/net/wifi
   west flash

STA mode - WPA2-PSK
====================

From the shell, connect to a WPA2 network (``-k 1`` = WPA2-PSK):

.. code-block:: console

   uart:~$ wifi connect -s <SSID> -k 1 -p <password>

Verify the connection and IP address:

.. code-block:: console

   uart:~$ wifi status
   uart:~$ net iface

STA mode - WPA3-SAE
====================

Connect to a WPA3 network (``-k 3`` = WPA3-SAE):

.. code-block:: console

   uart:~$ wifi connect -s <SSID> -k 3 -p <password>

Verify:

.. code-block:: console

   uart:~$ wifi status
   uart:~$ net iface

AP mode
=======

Start a soft AP from the shell (``-k 1`` = WPA2-PSK, ``-c`` = channel):

.. code-block:: console

   uart:~$ wifi ap enable -s ZephyrAP -k 1 -p zephyr123 -c 6

Check connected stations:

.. code-block:: console

   uart:~$ wifi ap stations

Tested with 4 clients connected simultaneously.

To stop the AP:

.. code-block:: console

   uart:~$ wifi ap disable

Wi-Fi testing
*************

Build and flash the Wi-Fi sample:

.. code-block:: console

   west build -b lp_em_cc35x1 samples/net/wifi
   west flash

STA mode - WPA2-PSK
====================

From the shell, connect to a WPA2 network (``-k 1`` = WPA2-PSK):

.. code-block:: console

   uart:~$ wifi connect -s <SSID> -k 1 -p <password>

Verify the connection and IP address:

.. code-block:: console

   uart:~$ wifi status
   uart:~$ net iface

STA mode - WPA3-SAE
====================

Connect to a WPA3 network (``-k 3`` = WPA3-SAE):

.. code-block:: console

   uart:~$ wifi connect -s <SSID> -k 3 -p <password>

Verify:

.. code-block:: console

   uart:~$ wifi status
   uart:~$ net iface

AP mode
=======

Start a soft AP from the shell (``-k 1`` = WPA2-PSK, ``-c`` = channel):

.. code-block:: console

   uart:~$ wifi ap enable -s ZephyrAP -k 1 -p zephyr123 -c 6

Check connected stations:

.. code-block:: console

   uart:~$ wifi ap stations

Tested with 4 clients connected simultaneously.

To stop the AP:

.. code-block:: console

   uart:~$ wifi ap disable

Bluetooth Mesh testing
**********************

Build and flash the mesh demo sample:

.. code-block:: console

   west build -b lp_em_cc35x1 samples/bluetooth/mesh_demo
   west flash

Device logs are emitted on the UART console.

.. note::

   Most of the lines shown below are at ``LOG_DBG`` level. Enable
   ``CONFIG_BT_LOG_LEVEL_DBG=y`` and ``CONFIG_BT_MESH_LOG_LEVEL_DBG=y``
   in the sample's ``prj.conf`` (or board overlay conf) to see them.

Provision the device
====================

Provision the device from a mesh provisioner. Expected device log:

.. code-block:: console

   <inf> bt_mesh_main: Primary Element: 0xNNNN
   <dbg> bt_mesh_main: Storing network information persistently
   <dbg> bt_mesh_net: Provisioned with primary address 0xNNNN
   <dbg> bt_mesh_net_keys: Stored NetKey value

Read composition data
====================

The provisioner reads Composition Data Page 0. Expected device log:

.. code-block:: console

   <dbg> bt_mesh_cfg_srv: Preparing Composition data page 0

Add an AppKey
=============

Add AppKey 0 under NetKey 0. Expected device log:

.. code-block:: console

   <dbg> bt_mesh_cfg_srv: AppIdx 0x0000 NetIdx 0x0000
   <dbg> bt_mesh_app_keys: Storing AppKey persistently
   <dbg> bt_mesh_app_keys: Stored AppKey bt/mesh/AppKey/0 value

Bind the AppKey to a model
==========================

Bind AppKey 0 to a model on element 0. Expected device log:

.. code-block:: console

   <dbg> bt_mesh_access: model key 0x0000
   <dbg> bt_mesh_access: Stored bt/mesh/s/0/bind value

Receive a model message
=======================

Send a message addressed to a bound model on the device. Expected
device log:

.. code-block:: console

   <dbg> bt_mesh_access: app_idx 0x0000 src 0xNNNN dst 0xNNNN

Programming and Debugging
*************************

The LP_EM_CC35X1 requires an external debug probe such as the LP-XDS110 or
LP-XDS110ET.

Currently there is no debug support in Zephyr for the LP_EM_CC35X1. Binaries can
be flashed with standard `west flash` command.

References
**********

.. _TI cc35x1 LaunchPad Product Page:
   https://www.ti.com/tool/LP-EM-CC35X1

.. _TI cc35x1 Product Page:
   https://www.ti.com/product/CC3551E
