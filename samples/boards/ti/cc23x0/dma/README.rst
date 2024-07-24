Overview
********

Sample code for cc23x0 DMA testing.



Building
********

west build -p always -b lp_em_cc2340r5 samples/boards/ti/cc23x0/dma



Output
******

*** Booting Zephyr OS build e876da0e0963 ***
[00:00:00.003,000] <dbg> os: k_sched_unlock: scheduler unlocked (0x200008c0:0)
[00:00:00.011,000] <inf> main: -------------------- DMA TEST 1 --------------------
[00:00:00.019,000] <dbg> dma_cc23x0: dma_cc23x0_config: Configured channel 6 for 20001a60 to 20000a60 (4096 bytes)
[00:00:00.029,000] <inf> main: dir = 0, busy = 0, pending = 4096
[[00:00:00.036,000] <dbg> dma_cc23x0: dma_cc23x0_isr: DMA transfer completed on channel 6
[00:00:00.044,000] <inf> main: On ch 6, 20001a60 to 20000a60 (4096 bytes)
src_data = 0 1 2 3 4 5 6 7 [...] 1023
dst_data = 0 1 2 3 4 5 6 7 [...] 1023
00:00:00.036,000] <inf> main: busy = 1, pending = 4032
[00:00:00.064,000] <inf> main: busy = 0, pending = 0
[00:00:00.070,000] <inf> main: TEST OK
[00:00:00.074,000] <inf> main: -------------------- DMA TEST 2 --------------------
[00:00:00.082,000] <dbg> dma_cc23x0: dma_cc23x0_reload: Reloaded channel 6 for 20001a60 to 20000a60 (4096 bytes)
[00:00:00.092,000] <inf> main: dir = 0, busy = 0, pending = 4096
[[00:00:00.099,000] <dbg> dma_cc23x0: dma_cc23x0_isr: DMA transfer completed on channel 6
[00:00:00.107,000] <inf> main: On ch 6, 20001a60 to 20000a60 (4096 bytes)
src_data = 1 2 3 4 5 6 7 8 [...] 1024
dst_data = 1 2 3 4 5 6 7 8 [...] 1024
00:00:00.099,000] <inf> main: busy = 1, pending = 4032
[00:00:00.127,000] <inf> main: busy = 0, pending = 0
[00:00:00.133,000] <inf> main: TEST OK
