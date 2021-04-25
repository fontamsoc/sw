// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#define STACKSZ		32 /* computed from -fstack-usage outputs */
#define BLKDEVADDR	0x0
#define NEXTLOADER	1 /* block index of next loader */
#define CSR_BASE	(0x8000000+0x1000) /* control interface address */
