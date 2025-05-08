.. _ble_mesh_blob_shell:

Bluetooth: Mesh BLOB Shell
##########################

Overview
********

This sample demonstrates Bluetooth Mesh's BLOB Models. It has the BLOB Client
Model and the BLOB Server Model in addition to standard mesh models, and it
supports provisioning over both the Advertising and the GATT Provisioning Bearers
(i.e. PB-ADV and PB-GATT). The Mesh BLOB Shell sample is designed for transfering
Binary Large OBjects (BLOB) in the mesh network using the standard BLOB Models.
It includes a serial console built to allow sending of commands, such as to setup
and start a BLOB transfer from a BLOB Client to a BLOB Server.


Requirements
************

* 2 boards with Bluetooth LE support
* Terminal Emulator (i.e., PuTTY or Tera Term)

User Interface
**************
LEDs:
   Used to identify the device being provisioned.

Terminal Emulator:
   Used to interacting with the sample's Mesh Shell.

Building and Running
********************

This sample can be found under :zephyr_file:`samples/bluetooth/mesh_blob_shell` in the
Zephyr tree.

See :ref:`bluetooth samples section <bluetooth-samples>` for details on how
to run the sample inside QEMU.

For other boards, build and flash the application as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/bluetooth/mesh_blob_shell
   :board: <board>
   :goals: flash
   :compact:

Refer to your :ref:`board's documentation <boards>` for alternative
flash instructions if your board doesn't support the ``flash`` target.

Interacting with the sample
***************************

1. Use PuTTY (or other serial terminal) to open a serial connection to the boards.
2. From there, you can send mesh commands to the boards using the Bluetooth Mesh
   Shell. For full reference on supported commands, refer to this
   `page <https://docs.zephyrproject.org/latest/connectivity/bluetooth/api/mesh/shell.html>`_.

To provision and test, continue to the sections below.

.. _provisioning-section-mesh-blob:

Provisioning
************

The sample can be provisioned into an existing mesh network with an external
provisioner device. The provisioner must give the device an Application Key and
bind it to the BLOB Client and BLOB Server models.

If using Texas Instrument's SimpleLink Connect Mobile App, refer to the
`SimpleLink Mesh Guide <_simplelink_connect_mesh_guide_mesh_blob>`_ for information on how
to provision and configure a mesh device.

Before getting started:
* Add an Application Key

The first device will act as the BLOB Client
* Bind the BLOB Client Model to the Application Key

The second device will act as the BLOB Server
* Bind the BLOB Server Model to the Application Key

.. _testing-section-mesh-blob:

Testing
*******

After provisioning the mesh nodes and configuring the BLOB models, you can start
a BLOB transfer from the BLOB Client to the BLOB Server. With serial terminals (PuTTY)
open for both of the nodes, follow the steps below:

NOTE: The following commands are just examples. To get more information on the
blob command parameters, refer to this
`page <https://docs.zephyrproject.org/latest/connectivity/bluetooth/api/mesh/shell.html>`_.

BLOB Server
===========
1. Initialize the mesh shell

   .. code-block:: bash

      mesh init

2. Prepare node for receiving a BLOB

   .. code-block:: bash

      mesh models blob srv rx 1 10

BLOB Client
===========
1. Initialize the mesh shell

   .. code-block:: bash

      mesh init

2. Add the BLOB Server as a target

   .. code-block:: bash

      mesh models blob cli target 0x0004

3. Start BLOB transfer

   .. code-block:: bash

      mesh models blob cli tx 1 4096 10 65 0x0000 pull 10

   NOTE: It may take several minutes to finish the BLOB transfer.
