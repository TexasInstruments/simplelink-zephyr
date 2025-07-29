.. zephyr:code-sample:: lvgl_simple_gui
   :name: LVGL GUI Tab Example
   :relevant-api: display_interface input_interface

   Display a three tabs with different widgets on each tab using LVGL

Overview
********
This sample application demonstrates the use of LVGL to create a simple GUI with three tabs,
each containing different widgets. Dispay a three tabs with different widgets on each tab.
The first tab contains a labels, the second tab contains a button and slide, and the third tab
contains a drop down menu and buttons. This samle application also demonstrates the use of
input devices, such as a pointer, to interact with the GUI.

* Pointer
      If your board has a touch panel controller
      (:dtcompatible:`zephyr,lvgl-pointer-input`), a button widget is displayed
      in the center of the screen. Otherwise a label widget is displayed.

Requirements
************

Display shield and a board which provides a configuration
for corresponding connectors, for example:

- :ref:`adafruit_2_4_tft` and :zephyr:board:`lm_em_cc35x1e`
- :ref:`st7789v_waveshare_240x320` and :zephyr:board:`lm_em_cc35x1e`

or
- :zephyr:board:`native_sim`
- `SDL2`_


Building and Running
********************

Example building for :zephyr:board:`nrf52840dk`:

.. zephyr-app-commands::
   :zephyr-app: samples/subsys/display/lvgl_simple_gui
   :board: lm_em_cc35x1e
   :shield: st7789v_waveshare_240x320
   :goals: build flash

Example building for :zephyr:board:`native_sim <native_sim>`:

.. zephyr-app-commands::
   :zephyr-app: samples/subsys/display/lvgl_simple_gui
   :board: native_sim
   :goals: build run

Alternatively, if building from a 64-bit host machine, the previous target
board argument may also be replaced by ``native_sim/native/64``.

References
**********

.. target-notes::

.. _LVGL Web Page: https://lvgl.io/
.. _SDL2: https://www.libsdl.org
