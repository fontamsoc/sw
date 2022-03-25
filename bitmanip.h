// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef BITMANIP_H
#define BITMANIP_H

// Left bit-rotation.
#define BITROL(VAL, BITSZ, CNT) ({                      \
	typeof(VAL) val = (VAL);                        \
	typeof(BITSZ) bitsz = (BITSZ);                  \
	typeof(CNT) cnt = (CNT);                        \
	typeof(VAL) mask = (((typeof(VAL))1<<bitsz)-1); \
	(((val<<cnt)|(val>>(bitsz-cnt)))&mask);         \
})

// Right bit-rotation.
#define BITROR(VAL, BITSZ, CNT) ({                      \
	typeof(VAL) val = (VAL);                        \
	typeof(BITSZ) bitsz = (BITSZ);                  \
	typeof(CNT) cnt = (CNT);                        \
	typeof(VAL) mask = (((typeof(VAL))1<<bitsz)-1); \
	(((val>>cnt)|(val<<(bitsz-cnt)))&mask);         \
})

#endif /* BITMANIP_H */
