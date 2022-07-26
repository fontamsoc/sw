// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_SPINLOCK_TYPES_H
#define __ASM_PU32_SPINLOCK_TYPES_H

typedef struct {
	volatile unsigned long lock[3]; // {lock, ticket current}
} arch_spinlock_t;
#define __ARCH_SPIN_LOCK_UNLOCKED { { 0, 0, 0 } }

typedef struct {
	volatile unsigned long lock[4]; // {lock, ticket current, readercnt}
} arch_rwlock_t;
#define __ARCH_RW_LOCK_UNLOCKED { { 0, 0, 0, 0 } }

#endif /* __ASM_PU32_SPINLOCK_TYPES_H */
