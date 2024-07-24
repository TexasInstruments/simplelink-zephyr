Overview
********

Sample code for cc23x0 crypto testing.



Building
********

west build -p always -b lp_em_cc2340r5 samples/boards/ti/cc23x0/crypto



Output
******

*** Booting Zephyr OS build ce801f4d37fc ***
[00:00:00.004,000] <dbg> os: k_sched_unlock: scheduler unlocked (0x20000280:0)
[00:00:00.013,000] <inf> main: Cipher Sample
[00:00:00.018,000] <inf> main: --------------- ECB Mode ---------------
[00:00:00.025,000] <dbg> crypto_cc23x0: crypto_cc23x0_ecb_encrypt: AES operation completed
[00:00:00.035,000] <dbg> crypto_cc23x0: crypto_cc23x0_ecb_encrypt: AES operation completed
[00:00:00.044,000] <inf> main: Output length (encryption): 32
[00:00:00.051,000] <inf> main: ECB mode ENCRYPT - Match
[00:00:00.057,000] <err> crypto_cc23x0: ECB decryption not supported by the hardware
[00:00:00.066,000] <inf> main: --------------- CTR Mode ---------------
[00:00:00.074,000] <dbg> crypto_cc23x0: crypto_cc23x0_ctr: AES operation completed
[00:00:00.083,000] <dbg> crypto_cc23x0: crypto_cc23x0_ctr: AES operation completed
[00:00:00.091,000] <dbg> crypto_cc23x0: crypto_cc23x0_ctr: AES operation completed
[00:00:00.100,000] <dbg> crypto_cc23x0: crypto_cc23x0_ctr: AES operation completed
[00:00:00.109,000] <inf> main: Output length (encryption): 64
[00:00:00.115,000] <inf> main: CTR mode ENCRYPT - Match
[00:00:00.122,000] <dbg> crypto_cc23x0: crypto_cc23x0_ctr: AES operation completed
[00:00:00.130,000] <dbg> crypto_cc23x0: crypto_cc23x0_ctr: AES operation completed
[00:00:00.139,000] <dbg> crypto_cc23x0: crypto_cc23x0_ctr: AES operation completed
[00:00:00.148,000] <dbg> crypto_cc23x0: crypto_cc23x0_ctr: AES operation completed
[00:00:00.156,000] <inf> main: Output length (decryption): 64
[00:00:00.163,000] <inf> main: CTR mode DECRYPT - Match
[00:00:00.169,000] <inf> main: --------------- CCM Mode ---------------
[00:00:00.177,000] <dbg> crypto_cc23x0: crypto_cc23x0_ccm_encrypt: Compute CMAC
[00:00:00.185,000] <dbg> crypto_cc23x0: crypto_cc23x0_cmac: AES operation completed (block 0)
[00:00:00.195,000] <dbg> crypto_cc23x0: crypto_cc23x0_cmac: AES operation completed (block 1)
[00:00:00.205,000] <dbg> crypto_cc23x0: crypto_cc23x0_cmac: AES operation completed (data)
[00:00:00.214,000] <dbg> crypto_cc23x0: crypto_cc23x0_cmac: AES operation completed (data)
[00:00:00.223,000] <dbg> crypto_cc23x0: crypto_cc23x0_ccm_encrypt: Encrypt data
[00:00:00.232,000] <dbg> crypto_cc23x0: crypto_cc23x0_ctr: AES operation completed
[00:00:00.241,000] <dbg> crypto_cc23x0: crypto_cc23x0_ctr: AES operation completed
[00:00:00.249,000] <dbg> crypto_cc23x0: crypto_cc23x0_ccm_encrypt: Encrypt tag
[00:00:00.257,000] <dbg> crypto_cc23x0: crypto_cc23x0_ctr: AES operation completed
[00:00:00.266,000] <inf> main: Output length (encryption): 23
[00:00:00.273,000] <inf> main: CCM mode ENCRYPT - Match
[00:00:00.279,000] <dbg> crypto_cc23x0: crypto_cc23x0_ccm_decrypt: Decrypt data
[00:00:00.288,000] <dbg> crypto_cc23x0: crypto_cc23x0_ctr: AES operation completed
[00:00:00.296,000] <dbg> crypto_cc23x0: crypto_cc23x0_ctr: AES operation completed
[00:00:00.305,000] <dbg> crypto_cc23x0: crypto_cc23x0_ccm_decrypt: Compute CMAC
[00:00:00.313,000] <dbg> crypto_cc23x0: crypto_cc23x0_cmac: AES operation completed (block 0)
[00:00:00.323,000] <dbg> crypto_cc23x0: crypto_cc23x0_cmac: AES operation completed (block 1)
[00:00:00.333,000] <dbg> crypto_cc23x0: crypto_cc23x0_cmac: AES operation completed (data)
[00:00:00.342,000] <dbg> crypto_cc23x0: crypto_cc23x0_cmac: AES operation completed (data)
[00:00:00.351,000] <dbg> crypto_cc23x0: crypto_cc23x0_ccm_decrypt: Encrypt tag
[00:00:00.360,000] <dbg> crypto_cc23x0: crypto_cc23x0_ctr: AES operation completed
[00:00:00.368,000] <dbg> crypto_cc23x0: crypto_cc23x0_ccm_decrypt: Check tag
[00:00:00.376,000] <inf> main: Output length (decryption): 23
[00:00:00.383,000] <inf> main: CCM mode DECRYPT - Match
[00:00:00.389,000] <inf> main: --------------- CTR-DRBG Mode ---------------
[00:00:00.398,000] <dbg> crypto_cc23x0: crypto_cc23x0_ctr_drbg: AES operation completed
[00:00:00.407,000] <dbg> crypto_cc23x0: crypto_cc23x0_ctr_drbg: AES operation completed
[00:00:00.416,000] <dbg> crypto_cc23x0: crypto_cc23x0_ctr_drbg: AES operation completed
[00:00:00.425,000] <dbg> crypto_cc23x0: crypto_cc23x0_ctr_drbg: AES operation completed
[00:00:00.434,000] <inf> main: Output length: 64
[00:00:00.439,000] <inf> main: CTR-DRBG - Match
