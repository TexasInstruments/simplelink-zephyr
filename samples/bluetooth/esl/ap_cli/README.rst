.. zephyr:code-sample:: bt_esl_ap_cli
   :name: Bluetooth LE Electronic Shelf Label (ESL) Access Point CLI
   :relevant-api: bt_gap bluetooth bt_gatt

   Demonstrate Bluetooth LE Electronic Shelf Label Access Point functionality with CLI interface.

Overview
********

This sample demonstrates the Bluetooth LE Electronic Shelf Label (ESL) Access Point
functionality with a command-line interface for managing ESL tags in a retail environment.

The Access Point uses Periodic Advertising with Responses (PAwR) to communicate with
multiple ESL tags simultaneously. It can discover, connect to, and manage various
ESL tag operations including display updates, LED control, and sensor information
retrieval.

Features
========

* **ESL Access Point Role**: Acts as central controller for ESL tags
* **Periodic Advertising with Responses (PAwR)**: Efficient communication with multiple tags
* **GATT Client Operations**: Service discovery and characteristic access
* **CLI Interface**: Interactive shell commands for testing and demonstration
* **Multi-tag Management**: Support for up to 4 tags per group, 1 group maximum which is configurable
* **Tag Operations**: Display updates, LED control, sensor data collection
* **Encrypted Advertising**: Support for secure ESL communications

CLI Commands
============

The sample provides the following shell commands under the ``esl_ap`` namespace:

* ``bt_on`` - Initialize Bluetooth stack
* ``start_scan`` - Start scanning for ESL tags
* ``stop_scan`` - Stop scanning operation
* ``discover <Grp ID> <ESL ID>`` - Discover ESL services on a specific tag
* ``start_padv`` - Start periodic advertising
* ``stop_padv`` - Stop periodic advertising
* ``connect <Grp ID> <ESL ID>`` - Connect to an ESL tag
* ``disconnect <Grp ID> <ESL ID>`` - Disconnect from an ESL tag
* ``led_control <Grp ID> <ESL ID> <led_index> <clr brightness> <flsh pattern> <rpt type> [delay in s]`` - Control tag LED
* ``display_update <Grp ID> <ESL ID> <display_index> <image index> [delay in s]`` - Update tag display
* ``sensor_info <Grp ID> <ESL ID>`` - Read sensor information from tag

Requirements
************

* A board with Bluetooth LE support
* A controller that supports:

  - Extended Advertising
  - Periodic Advertising with Responses (PAwR)
  - Encrypted Advertising Data (EAD)
  - Multiple connections (up to 5)

* One or more ESL tags (use the corresponding :zephyr:code-sample:`bt_esl_tag_cli` sample)

Building and Running
********************

This sample can be found under :file:`esl/platforms/nrf52/zephyr/ap_cli` in the
ESL source tree.

Build the sample:

.. zephyr-app-commands::
   :zephyr-app: esl/platforms/nrf52/zephyr/ap_cli
   :board: nrf52840dk_nrf52840
   :goals: build flash
   :compact:

Use with the corresponding :zephyr:code-sample:`bt_esl_tag_cli` sample that will
act as ESL tags responding to this Access Point.

Sample Output
=============

When the application starts, you will see output similar to:

.. code-block:: console

   *** Booting Zephyr OS build zephyr-v3.4.0 ***
   [00:00:04.510,223] <inf> bt_hci_core: HW Variant: nRF52x (0x0002)
   [00:00:04.510,253] <inf> bt_hci_core: Firmware: Standard Bluetooth controller (0x00) Version 137.20634 Build 2617349514
   [00:00:04.511,016] <inf> bt_hci_core: Identity: E5:B8:0C:D8:1F:FD (random)
   [00:00:04.511,047] <inf> bt_hci_core: HCI: version 6.0 (0x0e) revision 0x10f3, manufacturer 0x0059
   [00:00:04.511,077] <inf> bt_hci_core: LMP: version 6.0 (0x0e) subver 0x10f3
   [ESL PL]: Bluetooth initialized
   [ESL PL]: Local BD address E5:B8:0C:D8:1F:FD (random)
   [ESL PL]: Settings loaded successfully
   [ESL PL]: Auth info callbacks registered successfully
   [ESL PL]: ESL PL is initialized
   [APPL]: ESL PL initialized successfully
   ESL Module Version 001:000:000
   ==========================================
   uart:~$ esl_ap start_scan
   ESLP: Started Scanning
   [ESL PL]: Scanning successfully started
   uart:~$ esl_ap add_esl 1 E5B80CD81FFD 0 0
   Adding ESL device
   uart:~$

Testing
=======

1. Build and flash the Access Point CLI sample to one board
2. Build and flash the Tag CLI sample to one or more other boards
3. Use the CLI commands to:

   a. Initialize Bluetooth: ``esl_ap bt_on``
   b. Start scanning: ``esl_ap start_scan``
   c. Start periodic advertising: ``esl_ap start_padv``
   d. Connect to discovered tags and configure them
   e. Control tag displays and LEDs
   f. Read sensor data from tags

References
==========

* `Bluetooth SIG Electronic Shelf Label Service Specification <https://www.bluetooth.com/specifications/specs/electronic-shelf-label-service-1-0/>`_
* `Bluetooth SIG Electronic Shelf Label Profile Specification <https://www.bluetooth.com/specifications/specs/electronic-shelf-label-profile-1-0/>`_
* `Bluetooth samples <https://docs.zephyrproject.org/latest/samples/bluetooth/bluetooth.html>`_ for more Bluetooth LE examples
