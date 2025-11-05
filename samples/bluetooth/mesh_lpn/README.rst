.. _ble_mesh_lpn:

Bluetooth: Mesh LPN
###################

Overview
********

This sample demonstrates Bluetooth Mesh Low Power Node (LPN) functionality.
By default it supports the health server, configuration server and generic
on/off client models, and supports provisioning over both the
Advertising and the GATT Provisioning Bearers (i.e. PB-ADV and PB-GATT).
The application is designed to achieve the lowest power consumption possible
by default, therefore the serial interface, printk, etc have been disabled.
These can be re-enabled if during development by adjusting the appropriate Kconfig
options, but will increase power consumption.

On boards with buttons, a Generic OnOff Client model will send OnOff messages
to all nodes in the network when the button is pressed.

Requirements
************

* A board with Bluetooth LE support acting as a Low Power Node (this sample)
* A board with Bluetooth LE support to act as a friend (see :zephyr_file:`samples/bluetooth/mesh`)

Building and Running
********************

This sample can be found under :zephyr_file:`samples/bluetooth/mesh_lpn` in the
Zephyr tree.

See :ref:`bluetooth samples section <bluetooth-samples>` for details on how
to run the sample inside QEMU.

For other boards, build and flash the application as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/bluetooth/mesh_lpn
   :board: <board>
   :goals: flash
   :compact:

Refer to your :ref:`board's documentation <boards>` for alternative
flash instructions if your board doesn't support the ``flash`` target.

Provisioning
************

The sample can be provisioned into an existing mesh network with a
provisioner device. The provisioner must give the device an Application Key and
bind it to the Generic OnOff Client model.

If using Texas Instrument's SimpleLink Connect Mobile App, refer to the
`SimpleLink Mesh Guide <_simplelink_connect_mesh_guide_lpn>`_ for information on how
to provision and configure a mesh device.

Before getting started:
* Add an Application Key
* Create a group

On both of the devices
* Bind the Application Key to the Generic OnOff Client model
* Add Publication to a unicast or group address

Interacting with the sample
***************************

Once provisioned, button presses will be used to broadcast OnOff
messages to all nodes in the same network.
