.. _bluetooth-peripheral-multiple-adv:

Bluetooth: Peripheral Multiple Advertisements
#############################################

Overview
********

A simple application demonstrating BLE Peripheral with multiple advertising
sets. This sample initiates two advertising sets: one is connectable and the
other is non-connectable. The connectable advertising set is used to establish
connections with a Central device, while the non-connectable advertising set is
used to broadcast data to Observers.

Requirements
************

* A board with Bluetooth Low Energy with Extended Advertising support.

Building and Running
********************

This sample can be found under
:zephyr_file:`samples/bluetooth/peripheral_multiple_adv` in the Zephyr tree.

See :ref:`Bluetooth samples section <bluetooth-samples>` for details.

For other boards, build and flash the application as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/bluetooth/peripheral_multiple_adv
   :board: <board>
   :goals: flash
   :compact:

Refer to your :ref:`board's documentation <boards>` for alternative
flash instructions if your board doesn't support the ``flash`` target.

Interacting with the sample
***************************

Upon reset, the sample initiates two advertising sets: one connectable and the
other non-connectable.

Connectable Advertising Set
===========================

To establish connection to the connectable advertising set, you can use any
Central device, such as a smartphone.

Non-Connectable Advertising Set
===============================

You can use an Observer device to view the data broadcasted in the non-connectable
advertising packets. The data is broadcasted in the manufacturer specific data
that can be modified on-the-fly using the UART shell interface. Refer below for
an example:

.. code-block:: console

   $ bt adv set-mfg "hello world"