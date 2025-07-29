.. zephyr:code-sample:: ti-cc35xx-fwu
   :name: TI CC35xx firmware update

   Use the TI CC35xx PSA FWU API to inspect and update FWU slots.

Overview
********

This sample exposes TI CC35xx firmware update commands through the Zephyr
shell. It can list the configured FWU slots, download an update image over
HTTP, write it with the PSA FWU API, stage it, reboot into it, and accept or
reject the trial image.

The board must be provisioned with an OTA-capable flash layout before using
the FWU commands. A normal non-OTA initial programming layout has empty
secondary slots, so update commands cannot be used safely.

Building
********

Build the sample for the CC35xx launchpad:

.. code-block:: console

   west build -p always -b lp_em_cc35x1 samples/boards/ti/cc35xx/ti_fwu

Initial Programming
*******************

Before flashing the Zephyr application, run TI SimpleLink Wi-Fi Toolbox initial
programming with OTA enabled. The command below assumes that
``simplelink-wifi-toolbox`` is available in ``PATH`` and that the build
directory is ``build``:

.. code-block:: console

   simplelink-wifi-toolbox programmer -i XDS110 factory_programming \
     --activation_type sdk_example_key \
     --flash_type IS25WJ032F \
     --enable_ota \
     --vendor_out_file build/zephyr/zephyr.elf \
     --conf_bin_file build/zephyr/flash/cc35xx-conf.bin \
     --vendor_app_version 0.0.1.0 \
     --full_flash_erase \
     --rollback_protection no \
     --verbose

If multiple XDS110 probes are connected, pass the probe serial with
``-param1 <serial>`` after ``-i XDS110``.

Flashing and Running
********************

After initial programming, flash the sample normally:

.. code-block:: console

   west flash

The sample starts the shell and prints the FWU slot table. Connect Wi-Fi with
the standard Wi-Fi shell commands. DHCP starts automatically after the Wi-Fi
connection is established.

Host the update image on an HTTP server reachable by the board. For example:

.. code-block:: console

   python3 -m http.server 8000

Then use the sample FWU shell commands:

.. code-block:: console

   fwu list
   fwu download http://<http-server-ip>:8000/vendor_image.sign.bin vendor_2
   fwu install
   fwu reboot

After the updated image boots, accept it:

.. code-block:: console

   fwu accept

FWU Commands
************

The sample provides these shell commands:

``fwu list``
   List all known BL2, WSOC, and vendor image slots.

``fwu query <slot>``
   Print one slot state.

``fwu download <url> <slot>``
   Download an HTTP image and write it to the selected FWU slot.

``fwu install``
   Stage written candidates.

``fwu reboot``
   Request a FWU reboot.

``fwu accept``
   Accept the running trial image.

``fwu reject [error]``
   Reject the running trial image.

``fwu cancel <slot>``
   Cancel a pending write for a slot.

``fwu clean <slot>``
   Clean a slot with the PSA FWU API.
