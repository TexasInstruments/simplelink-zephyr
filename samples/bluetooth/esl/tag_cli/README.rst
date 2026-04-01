.. zephyr:code-sample:: bt_esl_tag_cli
   :name: Bluetooth LE Electronic Shelf Label (ESL) Tag CLI
   :relevant-api: bt_gap bluetooth bt_gatt

   Demonstrate Bluetooth LE Electronic Shelf Label Tag functionality with CLI interface.

Overview
********

This sample demonstrates the Bluetooth LE Electronic Shelf Label (ESL) Tag
functionality with a command-line interface for testing tag operations in
a retail environment.

The ESL Tag acts as a peripheral device that can be controlled by an ESL Access Point.
It supports various operations including display updates, LED control, sensor data
reporting, and synchronization via Periodic Advertising with Responses (PAwR).

The tag exposes ESL GATT services for configuration and control, and can respond
to periodic advertising events for efficient group communications.

Features
========

* **ESL Tag Role**: Acts as controllable electronic shelf label
* **GATT Server**: Exposes ESL service characteristics
* **Periodic Sync**: Synchronizes with Access Point periodic advertising
* **Display Support**: Multiple display management with image updates
* **LED Control**: Configurable LED patterns and timing
* **Sensor Integration**: Temperature, humidity, and other sensor data reporting
* **CLI Interface**: Interactive shell commands for testing and demonstration
* **Power Management**: Optimized for battery-powered operation
* **Encrypted Communications**: Support for secure ESL data exchange

CLI Commands
============

The sample provides the following shell commands under the ``esl_tag`` namespace:

* ``bt_on`` - Initialize Bluetooth stack and start advertising
* ``start`` - Start ESL tag advertising
* ``stop`` - Stop ESL tag advertising
* ``config_image <flag>`` - Configure display image settings
* ``config_bs`` - Configure Basic State service needed bit
* ``get_time`` - Get current synchronized time
* ``disconnection_on_fr <mode>`` - Set disconnection behavior on factory reset
* ``reset`` - Perform ESL tag factory reset

GATT Services
=============

The ESL Tag exposes the following GATT services:

* **Generic Access Profile (GAP)** - Device name and appearance
* **Generic Attribute Profile (GATT)** - Service changed indications
* **Battery Service (BAS)** - Battery level reporting
* **Electronic Shelf Label Service** - ESL-specific characteristics:

  - ESL Control Point - Command and status interface
  - Display Information - Display capabilities and configuration
  - LED Information - LED capabilities and control
  - Sensor Information - Environmental sensor data
  - Image Information - Supported image formats and storage
  - ESL Address - Unique ESL identifier
  - AP Sync Key Material - Key material for Access Point synchronization
  - ESL Response Key Material - Key material for responding to Access Point commands
  - ESL Current Absolute Time - Current absolute time information

Display and LED Support
=======================

The tag supports:

* **Multiple Displays**: Up to 4 configurable displays with different resolutions
* **Display Types**: Black/white. See `ESL display types
  <https://bitbucket.org/bluetooth-SIG/public/src/main/assigned_numbers/profiles_and_services/esl/display_types.yaml>`_
  for the full list of supported display types.
* **LED Control**: Individual LED control with patterns and timing
* **Image Management**: Upload and display of custom images
* **Sensor Integration**: Built-in sensors for environmental monitoring

Requirements
************

* A board with Bluetooth LE support
* A controller that supports:
  - Extended Advertising
  - Periodic Advertising Sync Transfer(PAST)
  - Periodic Advertising with Responses (PAwR)
* A host that supports:
  - Encrypted Advertising Data (EAD)
  - GATT Server operations

* An ESL Access Point (use the corresponding :zephyr:code-sample:`bt_esl_ap_cli` sample)
* Optional: Display hardware, LEDs, and sensors for full functionality

Building and Running
********************

This sample can be found under :file:`esl/platforms/nrf52/zephyr/tag_cli` in the
ESL source tree.

Build the sample:

.. zephyr-app-commands::
   :zephyr-app: esl/platforms/nrf52/zephyr/tag_cli
   :board: nrf52840dk_nrf52840
   :goals: build flash
   :compact:

Use with the corresponding :zephyr:code-sample:`bt_esl_ap_cli` sample that will
act as the ESL Access Point controlling this tag.

Sample Output
=============

When the application starts, you will see output similar to:

.. code-block:: console

   *** Booting Zephyr OS build zephyr-v3.4.0 ***
   [00:00:25.006,378] <inf> bt_hci_core: HW Platform: Nordic Semiconductor (0x0002)
   [00:00:25.006,408] <inf> bt_hci_core: HW Variant: nRF52x (0x0002)
   [00:00:25.006,439] <inf> bt_hci_core: Firmware: Standard Bluetooth controller (0x00) Version 137.20634 Build 2617349514
   [00:00:25.007,202] <inf> bt_hci_core: Identity: C4:74:8F:4A:E6:B7 (random)
   [00:00:25.007,232] <inf> bt_hci_core: HCI: version 6.0 (0x0e) revision 0x10f3, manufacturer 0x0059
   [00:00:25.007,263] <inf> bt_hci_core: LMP: version 6.0 (0x0e) subver 0x10f3
   [ESL PL]: Bluetooth initialized
   [ESL PL]: Local BD address C4:74:8F:4A:E6:B7 (random)
   [ESL PL]: Settings loaded successfully
   [ESL PL]: database registration success
   [ESL PL]: Auth info callbacks registered successfully
   [ESL PL]: Device name updated
   [ESL PL]: ESL TAG Periodic Advertising callbacks register
   [ESL PL]: ESL PL is initialized
   [APPL]: ESL PL initialized successfully
   ESL Module Version 001:000:000
   ==========================================
   [ESL PL]: Bluetooth stack initialization started
   [ESL PL]: <- bt_esl_init_pl
   uart:~$ esl_tag start
   Starting Advertisement
   [ESL PL]: No advertising set to stop
   [APPL]: bt_esl_stop_advertise_pl retval - 0xFFFF
   [ESL PL]: Device name updated
   [ESL PL]: Advertising successfully started

When connected by an Access Point, you'll see connection events and command processing:

.. code-block:: console

   [ESL PL]: Connection complete received for ADDR: 2F:AD:D8:E8:07:C0, TYPE: 00 (0x00)
   [APPL]: esl_connect_ind_cb
   [APPL]: Connected to ESL tag with address: ADDR: 2F:AD:D8:E8:07:C0, TYPE: 00
   [ESL PL]: Pairing complete ADDR: 2F:AD:D8:E8:07:C0, TYPE: 00(bonded 1)
   [APPL_ESL]: appl_esl_tag_ping_req_cb
   [APPL_ESL]: Ping command received from : ADDR: 2F:AD:D8:E8:07:C0, TYPE: 00
   [ESL PL]: Notification Timer started successfully
   [ESL PL]: <- control_point_write_handler_pl
   [ESL PL]: Notification sent successfully
   [ESL PL]: Control Point Notification complete for ADDR: 2F:AD:D8:E8:07:C0, TYPE: 00

Testing
=======

1. Build and flash the Tag CLI sample to one or more boards
2. Build and flash the Access Point CLI sample to another board
3. Use the tag CLI commands to configure the tag
4. Use the Access Point to discover and control the tag:

   a. Tag: ``esl_tag bt_on`` - Initialize and start advertising
   b. AP: ``esl_ap bt_on`` - Initialize Access Point
   c. AP: ``esl_ap start_scan`` - Discover the tag
   d. AP: ``esl_ap connect <addr>`` - Connect to the tag
   e. AP: ``esl_ap ping <Grp ID> <ESL ID>`` - Ping

Tag Configuration
=================

The tag can be configured for different retail scenarios:

* **Product Information Display**: Show prices, descriptions, promotions
* **Environmental Monitoring**: Report temperature, humidity conditions
* **Battery Status**: Monitor and report power levels
* **Inventory Tracking**: Respond to stock management commands
* **Group Operations**: Participate in synchronized updates

References
==========

* `Bluetooth SIG Electronic Shelf Label Service Specification <https://www.bluetooth.com/specifications/specs/electronic-shelf-label-service-1-0/>`_
* `Bluetooth SIG Electronic Shelf Label Profile Specification <https://www.bluetooth.com/specifications/specs/electronic-shelf-label-profile-1-0/>`_
* `Bluetooth samples <https://docs.zephyrproject.org/latest/samples/bluetooth/bluetooth.html>`_ for more Bluetooth LE examples
