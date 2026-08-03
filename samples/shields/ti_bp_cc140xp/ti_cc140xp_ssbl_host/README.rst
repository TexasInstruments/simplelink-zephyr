.. _ti_cc140xp_ssbl_host_sample:

TI CC140xP SSBL Host Sample
############################

Overview
********

This sample demonstrates a firmware-update host for the TI CC140xP
transceiver's Secure Serial Bootloader (SSBL). Unlike the other
``ti_cc140xp_*`` samples, it does **not** use the ``ti_cc140xp`` Zephyr driver
or the TRX_Host command API — SSBL is a completely different wire protocol
from normal TRX operation, so this sample talks to the transceiver directly
over raw SPI and GPIO (manual chip select, reset, and trigger pulses).

At boot, the sample runs ``performSsblDownloadSequence()`` unconditionally,
which resets the transceiver into its bootloader, verifies connectivity,
and streams a new firmware image into the TRX. It expects the firmware
update blob to already be present at a fixed flash partition
(``ssbl_fw_partition``, offset ``0x77000`` on the LP_EM_CC2745R10_Q1 board)
before it runs — see `Preparing the firmware blob`_ below. This is the
production variant: it has no buttons and no test/diagnostic branching, and
is meant to be used as one-shot field-update tooling.

Optional customer public-key programming (for signed firmware images) is
supported via the ``ssblDownload_t`` struct in ``src/main.c``, but is
disabled by default — programming a customer key is a one-time, irreversible
operation. Read the warning comment in ``main.c`` before enabling it.

Building and Running
********************

This sample can be built for any board with SPI and GPIO support. A device
tree overlay must define ``reset-gpios`` and ``trigger-gpios`` under
``zephyr,user``, and an ``ssbl_fw_partition`` flash partition sized to hold
the firmware update blob.

On the LP_EM_CC2745R10_Q1, this sample still requires the
:ref:`ti_bp_cc140xp` shield even though it doesn't use the shield's
ti_cc140xp device node - the shield's board overlay disables ``led1`` and
``spi1``, both of which conflict with this sample's DIO16 reset line on
this board:

.. code-block:: console

   west build -b lp_em_cc2745r10_q1/cc2745r10_q1 --shield ti_bp_cc140xp samples/shields/ti_bp_cc140xp/ti_cc140xp_ssbl_host
   west flash

The sample uses the following pin assignments on the LP_EM_CC2745R10_Q1
(shared with the other ti_cc140xp samples' TRX wiring, since there is only
one physical reset line and one CS line on the board):

  - SPI0 SCLK: DIO3
  - SPI0 POCI: DIO4
  - SPI0 PICO: DIO5
  - CS / trigger: DIO7
  - RESET:         DIO16
  - INT (diagnostic only, unused by SSBL): DIO28

Preparing the firmware blob
============================

This sample only performs the *host* side of the update — it does not embed
a firmware image. Before running it, program the TRX firmware update blob
(e.g. ``trx_fw_update_final.bin``) into the ``ssbl-fw-update`` flash
partition at offset ``0x77000`` using UniFlash, as a separate load step
after flashing this sample's own image. See the UniFlash flashing guide for
the exact steps (enable "Do not erase before program load" in Settings &
Utilities, then load the binary at that offset).

Sample Output
=============

.. code-block:: console

   *** Booting Zephyr OS build v3.7.0 ***
   DIO28 (INT, unused by SSBL) at boot: 1
   Starting the Secure Serial Bootloader(SSBL) Host example
   This example assumes 'trx_fw_update_final.bin' has already been programmed at the ssbl-fw-update flash partition, offset 0x77000.
   Download sequence completed successfully :) !!
