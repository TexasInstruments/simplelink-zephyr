.. zephyr:code-sample:: cc35xx-pm-wifi
   :name: CC35xx WiFi power management
   :relevant-api: subsys_pm_device net_if

   Demonstrate CC35xx WiFi power save with Zephyr power management.

Overview
********

This sample connects the CC35xx station interface to an access point, acquires an
IPv4 address with DHCP, configures WiFi firmware power save, and then enters a
low-power loop.

Two low-power modes are available:

* ``PM_WIFI_LOW_POWER_PS`` keeps the host active and enables only WiFi firmware
  power save. Use this mode when the station must remain reachable for traffic
  such as ping.
* ``PM_WIFI_LOW_POWER_SUSPEND`` enables WiFi power save and releases the host
  standby lock for a 5 second sleep window. The sample then re-locks standby for
  a 30 second active window.

Configuration
*************

Set the access point credentials with Kconfig:

.. code-block:: cfg

   CONFIG_PM_WIFI_SSID="ssid_name"
   CONFIG_PM_WIFI_PSK="password"

The default low-power mode is ``PM_WIFI_LOW_POWER_SUSPEND``. To build the sample
in firmware power-save-only mode, add:

.. code-block:: cfg

   CONFIG_PM_WIFI_LOW_POWER_PS=y

Building and Running
********************

Build and flash the sample for the LP-EM-CC35x1 board:

.. zephyr-app-commands::
   :zephyr-app: samples/boards/ti/cc35xx/pm_wifi
   :board: lp_em_cc35x1
   :goals: build flash
   :compact:

Expected output includes:

.. code-block:: console

   pm_wifi sample: boot
   wifi: connecting to "ssid_name"
   wifi: connected
   wifi: ipv4 acquired 192.0.2.10
   pm_wifi: low-power mode=suspend
   pm_wifi: entering 30s active / 5s sleep loop
