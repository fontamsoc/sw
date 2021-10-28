// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#define STACKSZ         16 /* computed from -fstack-usage outputs */
#define BLKDEVADDR      (0x0 /* By convention, the first block device is located at 0x0 */)
#define DEVTBLADDR      (0x200 /* By convention, the device table is located at 0x200 */)
#define BIOSADDR        0x1000
#define KERNELADDR      0x8000 /* must match corresponding constant in the kernel source-code */
#define KSLVAL          (KERNELADDR*16)
#define BIOSPART        1
