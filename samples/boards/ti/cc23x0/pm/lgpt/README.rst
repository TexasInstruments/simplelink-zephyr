.. zephyr:code-sample:: alarm
   :name: Counter Alarm
   :relevant-api: counter_interface

   Implement an alarm application using the counter API.

Overview
********
This sample provides an example of alarm application using :ref:`counter API <counter_api>`.
It sets an alarm with an initial delay of 2 seconds. At each alarm
expiry, a new alarm is configured with a delay multiplied by 2.

.. note::
   In case of 1Hz frequency (RTC for example), precision is 1 second.
   Therefore, the sample output may differ in 1 second

Requirements
************

This sample requires the support of a timer IP compatible with alarm setting.

References
**********

- :ref:`lp_em_cc2340r5`

Building and Running
********************

 .. zephyr-app-commands::
    :zephyr-app: samples/drivers/counter/alarm
    :host-os: unix
    :board: lp_em_cc2340r5
    :goals: run
    :compact:

Sample Output
=============

 .. code-block:: console

[16:52:38:282] *** Booting Zephyr OS build v3.7.0-rc3-160-g104f001b1b06 ***␍␊
[11:24:24:347] ␍␊
[11:24:24:347]  --- Counter sample ---␍␊
[11:24:24:351] ␍␊
[11:24:24:351]  --- All LGPT timers are ready ---␍␊
[11:24:24:354]  set_alarm base (40060000)@(0) IMASK (100)␍␊
[11:24:24:357] [LGPT0]--[0] Set alarm in 100000 us (18750 ticks)␍␊
[11:24:24:363]  set_alarm base (40060000)@(1) IMASK (300)␍␊
[11:24:24:365] [LGPT0]--[1] Set alarm in 110000 us (20625 ticks)␍␊
[11:24:24:369]  set_alarm base (40060000)@(2) IMASK (700)␍␊
[11:24:24:373] [LGPT0]--[2] Set alarm in 120000 us (22500 ticks)␍␊
[11:24:24:377]  set_alarm base (40060000)@(3) IMASK (701)␍␊
[11:24:24:381] [LGPT0]--[3] Set alarm in 130000 us (24375 ticks)␍␊
[11:24:24:385]  set_alarm base (40061000)@(0) IMASK (100)␍␊
[11:24:24:390] [LGPT1]--[0] Set alarm in 140000 us (26250 ticks)␍␊
[11:24:24:394]  set_alarm base (40061000)@(1) IMASK (300)␍␊
[11:24:24:399] [LGPT1]--[1] Set alarm in 150000 us (28125 ticks)␍␊
[11:24:24:402]  set_alarm base (40061000)@(2) IMASK (700)␍␊
[11:24:24:405] [LGPT1]--[2] Set alarm in 160000 us (30000 ticks)␍␊
[11:24:24:410]  set_alarm base (40061000)@(3) IMASK (701)␍␊
[11:24:24:414] [LGPT1]--[3] Set alarm in 170000 us (31875 ticks)␍␊
[11:24:24:419]  set_alarm base (40062000)@(0) IMASK (100)␍␊
[11:24:24:423] [LGPT2]--[0] Set alarm in 180000 us (33750 ticks)␍␊
[11:24:24:428]  set_alarm base (40062000)@(1) IMASK (300)␍␊
[11:24:24:430] [LGPT2]--[1] Set alarm in 190000 us (35625 ticks)␍␊
[11:24:24:434]  set_alarm base (40062000)@(2) IMASK (700)␍␊
[11:24:24:439] [LGPT2]--[2] Set alarm in 200000 us (37500 ticks)␍␊
[11:24:24:443]  set_alarm base (40062000)@(3) IMASK (701)␍␊
[11:24:24:447] [LGPT2]--[3] Set alarm in 210000 us (39375 ticks)␍␊
[11:24:24:451]  set_alarm base (40063000)@(0) IMASK (100)␍␊
[11:24:24:455] [LGPT3]--[0] Set alarm in 220000 us (41250 ticks)␍␊
[11:24:24:459]  set_alarm base (40063000)@(1) IMASK (300)␍␊
[11:24:24:465] [LGPT3]--[1] Set alarm in 230000 us (43125 ticks)␍␊
[11:24:24:468]  set_alarm base (40063000)@(2) IMASK (700)␍␊
[11:24:24:471] [LGPT3]--[2] Set alarm in 240000 us (45000 ticks)␍␊
[11:24:24:476]  set_alarm base (40063000)@(3) IMASK (701)␍␊
[11:24:24:479] [LGPT3]--[3] Set alarm in 250000 us (46875 ticks)␍␊
[11:24:24:484] ␍␊
[11:24:24:484] ISR -> LGP␍␊
[11:24:24:486] ISR -␍␊
[11:24:24:486] ISR -> LGPT[40063000] RIS[100] MIS[100] ISR[100]␍␊
[11:24:24:492] !!! Alarm !!!␍␊
[11:24:24:492] [0] chan(0) Set alarm in 440000 us (82500 ticks)␍␊
[11:24:24:496]  set_alarm base (40063000)@(0) IMASK (701)␍␊
[11:24:24:500] ␍␊
[11:24:24:500] ISR -> LGPT[40063000] RIS[603] MIS[601] ISR[601]␍␊
[11:24:24:504] !!! Alarm !!!␍␊
[11:24:24:506] [1] chan(3) Set alarm in 500000 us (93750 ticks)␍␊
[11:24:24:510]  set_alarm base (40063000)@(3) IMASK (101)␍␊
[11:24:24:513] !!! Alarm !!!␍␊
[11:24:24:515] [2] chan(1) Set alarm in 460000 us (86250 ticks)␍␊
[11:24:24:519]  set_alarm base (40063000)@(1) IMASK (301)␍␊
[11:24:24:523] !!! Alarm !!!␍␊
[11:24:24:525] [3] chan(2) Set alarm in 480000 us (90000 ticks)␍␊
[11:24:24:529]  set_alarm base (40063000)@(2) IMASK (701)␍␊
[11:24:24:534] ␍␊
[11:24:24:534] ISR -> LGPT[40063000] RIS[703] MIS[701] ISR[701]␍␊
[11:24:24:537] !!! Alarm !!!␍␊
[11:24:24:539] [4] chan(3) Set alarm in 1000000 us (187500 ticks)␍␊
[11:24:24:542]  set_alarm base (40063000)@(3) IMASK (1)␍␊
[11:24:24:547] !!! Alarm !!!␍␊
[11:24:24:548] [5] chan(0) Set alarm in 880000 us (165000 ticks)␍␊
[11:24:24:552]  set_alarm base (40063000)@(0) IMASK (101)␍␊
[11:24:24:556] !!! Alarm !!!␍␊
[11:24:24:557] [6] chan(1) Set alarm in 920000 us (172500 ticks)␍␊
[11:24:24:562]  set_alarm base (40063000)@(1) IMASK (301)␍␊
[11:24:24:565] !!! Alarm !!!␍␊
[11:24:24:568] [7] chan(2) Set alarm in 960000 us (180000 ticks)␍␊
[11:24:24:571]  set_alarm base (40063000)@(2) IMASK (701)␍␊
[11:24:24:575] ␍␊
[11:24:24:575] ISR -> LGPT[40063000] RIS[703] MIS[701] ISR[701]␍␊
[11:24:24:579] !!! Alarm !!!␍␊
[11:24:24:581] [8] chan(3) Set alarm in 2000000 us (375000 ticks)␍␊
[11:24:24:585]  set_alarm base (40063000)@(3) IMASK (1)␍␊
[11:24:24:589] !!! Alarm !!!␍␊
[11:24:24:591] [9] chan(0) Set alarm in 1760000 us (330000 ticks)␍␊
[11:24:24:594]  set_alarm base (40063000)@(0) IMASK (101)␍␊
[11:24:24:599] !!! Alarm !!!␍␊
[11:24:24:600] [10] chan(1) Set alarm in 1840000 us (345000 ticks)␍␊
[11:24:24:604]  set_alarm base (40063000)@(1) IMASK (301)␍␊
[11:24:24:608] !!! Alarm !!!␍␊
[11:24:24:610] [11] chan(2) Set alarm in 1920000 us (360000 ticks)␍␊
[11:24:24:614]  set_alarm base (40063000)@(2) IMASK (701)␍␊
    <repeats until reach 1000 isr>
[11:24:35:140] [STOP]  BASE[40061000] ␍␊
[11:24:35:142] [STOP]  BASE[40060000] ␍␊
[11:24:35:144] [STOP]  BASE[40062000] ␍␊
[11:24:35:147] [STOP]  BASE[40062000] ␍␊
[11:24:35:149] [STOP]  BASE[40062000] ␍␊
[11:24:35:152] [STOP]  BASE[40062000] ␍␊
[11:24:35:154] [STOP]  BASE[40063000] ␍␊
[11:24:35:156] [STOP]  BASE[40063000] ␍␊
[11:24:35:158] [STOP]  BASE[40063000] ␍␊
[11:24:35:160] [STOP]  BASE[40063000] ␍␊



