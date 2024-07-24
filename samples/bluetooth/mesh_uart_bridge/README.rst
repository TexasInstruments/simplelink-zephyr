.. _ble_mesh_uart_bridge:

Bluetooth: Mesh UART Bridge
###########################

Overview
********

This sample demonstrates Texas Instruments' UART Bridge Model. For provisioning,
it supports both the Advertising and the GATT Provisioning Bearers (i.e. PB-ADV
and PB-GATT). The UART Bridge Model supports sending of payloads (up to 377
bytes) to mesh nodes in the network. A UART shell is built to allow interaction
with the sample.


Requirements
************

* 2 or more boards with Bluetooth LE support
* Terminal Emulator (i.e., PuTTY or Tera Term)

User Interface
**************
LEDs:
   Used to identify the device being provisioned.

Terminal Emulator:
   Used to interacting with the sample's Mesh UART Shell.

Building and Running
********************

This sample can be found under :zephyr_file:`samples/bluetooth/mesh_uart_bridge` in the
Zephyr tree.

See :ref:`bluetooth samples section <bluetooth-samples>` for details on how
to run the sample inside QEMU.

For other boards, build and flash the application as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/bluetooth/mesh_uart_bridge
   :board: <board>
   :goals: flash
   :compact:

Refer to your :ref:`board's documentation <boards>` for alternative
flash instructions if your board doesn't support the ``flash`` target.

Interacting with the sample
***************************

1. Use PuTTY (or other serial terminal) to open a serial connection to the boards.
2. From there, you can send commands to the boards using the UART Shell

To provision and test, continue to the sections below.

.. _provisioning-section-uart-bridge:

Provisioning
************

The sample can be provisioned into an existing mesh network with an external
provisioner device. The provisioner must give the device an Application Key and
bind it to the UART Bridge Model. It may show up as `Vendor Model` on the
provisioner, but it will have the ID of `0x0D0010`.

If using Texas Instrument's SimpleLink Connect Mobile App, refer to the
`SimpleLink Mesh Guide <_simplelink_connect_mesh_guide_uart_bridge>`_ for information on how
to provision and configure a mesh device.

Before getting started:
* Add an Application Key
* Create a group

On both of the devices
* Bind the Application Key to the UART Bridge Model
* Add Publication to a group address
* Add Subscription to the same group address

.. _remote-provisioning-section:

Remote Provisioning
*******************

The sample can be used to remotely provision other devices using the Remote
Provisioning Server Model. Before remote provisioning can occur, the sample must
be provisioned, application keys bound, and connected to the newly formed mesh
network. To perform remote provisioning, follow the steps below:

1. Go to the Proxy tab in the Mobile App.
2. In the Proxy tab, select check that the mesh_uart_bridge device is selected.
3. Go to the Network tab.
4. Click the + button to look for new devices.
5. When a new device is found, wait for the bi-directional arrow icon to appear.
6. Click the bi-directional arrow icon. You will then be prompted to choose a bearer.
7. Select the 'PB Remote via' option.
8. Continue the provisioning and configuration process as normal.

.. _testing-section-uart-bridge:

Testing
*******

After provisioning the mesh nodes and configuring the UART Bridge Models, you can
start a payload transfer from one device to the network. With serial terminals (PuTTY)
open for both of the nodes, follow the steps below:

Mesh UART Bridge #1
===================
1. Publish payload to the model

   .. code-block:: bash

      bridge payload "hello world from mesh uart bridge 1"

Mesh UART Bridge #2
===================
1. Publish payload to the model

   .. code-block:: bash

      bridge payload "hi world from mesh uart bridge 2"
