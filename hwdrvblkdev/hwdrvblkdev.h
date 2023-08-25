// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef HWDRVBLKDEV_H
#define HWDRVBLKDEV_H

// Structure representing a block device.
// Before initializing the device using
// init(), the field addr must be valid.
typedef struct {
	// Device address.
	void* addr;
	// Capacity in block count.
	unsigned long blkcnt;
} hwdrvblkdev;

// Commands.
#define HWDRVBLKDEV_RESET	(0*__SIZEOF_POINTER__)
#define HWDRVBLKDEV_SWAP	(1*__SIZEOF_POINTER__)
#define HWDRVBLKDEV_READ	(2*__SIZEOF_POINTER__)
#define HWDRVBLKDEV_WRITE	(3*__SIZEOF_POINTER__)

// Status.
#define HWDRVBLKDEV_POWEROFF	0
#define HWDRVBLKDEV_READY	1
#define HWDRVBLKDEV_BUSY	2
#define HWDRVBLKDEV_ERROR	3

// Block size in bytes.
#define BLKSZ 512

// Function called before hwdrvblkdev_isrdy() returns busy (0).
static void (*hwdrvblkdev_isbsy)(void) = (void *)0;

// Read block device status.
// Returns 1 if ready, 0 if busy, -1 if error/poweroff.
static signed long hwdrvblkdev_isrdy (hwdrvblkdev *dev) {
	unsigned long n = 0;
	__asm__ __volatile__ (
		"ldst %0, %1"
		: "+r" (n)
		: "r" (dev->addr+HWDRVBLKDEV_RESET)
		: "memory");
	if (n == HWDRVBLKDEV_POWEROFF || n == HWDRVBLKDEV_ERROR)
		return -1;
	return ((n == HWDRVBLKDEV_READY) ? 1 : (hwdrvblkdev_isbsy ? (hwdrvblkdev_isbsy(), 0) : 0));
}

static void *hwdrvblkdev_read_ptr_saved;
static unsigned long hwdrvblkdev_read_idx_saved;
static void *hwdrvblkdev_write_ptr_saved;
static unsigned long hwdrvblkdev_write_idx_saved;

// Initialize the block device at the address given through
// the argument dev->addr; the field dev->blkcnt get initialized
// by this function.
// As part of the initialization, the block given by the argument idx gets loaded.
// On success returns 1 otherwise 0.
static unsigned long hwdrvblkdev_init (hwdrvblkdev *dev, unsigned long idx) {
	void* addr = dev->addr;
	// Reset the controller.
	__asm__ __volatile__ (
		"ldst %0, %1"
		: "+r" ((unsigned long){1})
		: "r" (addr+HWDRVBLKDEV_RESET)
		: "memory");
	// Read status until ready is returned.
	signed long isrdy;
	do {
		if ((isrdy = hwdrvblkdev_isrdy (dev)) < 0)
			return (dev->blkcnt = 0);
	} while (!isrdy);
	// Retrieve the capacity.
	dev->blkcnt = idx;
	__asm__ __volatile__ (
		"ldst %0, %1"
		: "+r" (dev->blkcnt)
		: "r" (addr+HWDRVBLKDEV_READ)
		: "memory");
	// Read status until ready is returned.
	do {
		if ((isrdy = hwdrvblkdev_isrdy (dev)) < 0)
			return (dev->blkcnt = 0);
	} while (!isrdy);
	// Present the loaded data in the physical memory.
	__asm__ __volatile__ (
		"ldst %%sr, %0"
		:: "r" (addr+HWDRVBLKDEV_SWAP)
		: "memory");
	hwdrvblkdev_read_ptr_saved = (void *)-1;
	hwdrvblkdev_read_idx_saved = -1;
	hwdrvblkdev_write_ptr_saved = (void *)-1;
	hwdrvblkdev_write_idx_saved = -1;
	return 1;
}

void *memcpy (void *dest, const void *src, size_t count);

// Read a block from the block device into the buffer given by the argument ptr.
// Argument nxt is used to initiate the next block read while retrieving block read.
// The index of the block to read is given by the argument idx, where a block is BLKSZ bytes.
// The block device must be ready. Returns 1 if block was read, otherwise 0 indicating retry is needed.
static unsigned long hwdrvblkdev_read (hwdrvblkdev *dev, void* ptr, unsigned long idx, unsigned long nxt) {
	void* addr = dev->addr;
	if (ptr == hwdrvblkdev_read_ptr_saved && idx == hwdrvblkdev_read_idx_saved)
		goto resume;
	hwdrvblkdev_write_ptr_saved = (void *)-1;
	hwdrvblkdev_write_idx_saved = -1;
	// Initiate the block read.
	__asm__ __volatile__ (
		"ldst %0, %1"
		: "+r" ((unsigned long){idx})
		: "r" (addr+HWDRVBLKDEV_READ)
		: "memory");
	hwdrvblkdev_read_ptr_saved = ptr;
	hwdrvblkdev_read_idx_saved = idx;
	return 0;
	resume:
	// Present the loaded data in the physical memory.
	__asm__ __volatile__ (
		"ldst %%sr, %0"
		:: "r" (addr+HWDRVBLKDEV_SWAP)
		: "memory");
	if (nxt) {
		idx += 1;
		// Initiate the next block read.
		__asm__ __volatile__ (
			"ldst %0, %1"
			: "+r" ((unsigned long){idx})
			: "r" (addr+HWDRVBLKDEV_READ)
			: "memory");
		hwdrvblkdev_read_ptr_saved = (ptr + BLKSZ);
		hwdrvblkdev_read_idx_saved = idx;
	} else {
		hwdrvblkdev_read_ptr_saved = (void *)-1;
		hwdrvblkdev_read_idx_saved = -1;
	}
	// Retrieve loaded data.
	memcpy (ptr, addr, BLKSZ);
	return 1;
}

// Write a block to the block device from the buffer given by the argument ptr.
// Argument nxt is used to fill controller up with next data while writing a block.
// The index of the block to write is given by the argument idx, where a block is BLKSZ bytes.
// The block device must be ready.
static void hwdrvblkdev_write (hwdrvblkdev *dev, void* ptr, unsigned long idx, unsigned long nxt) {
	void* addr = dev->addr;
	if (ptr == hwdrvblkdev_write_ptr_saved && idx == hwdrvblkdev_write_idx_saved)
		goto resume;
	hwdrvblkdev_read_ptr_saved = (void *)-1;
	hwdrvblkdev_read_idx_saved = -1;
	memcpy (addr, ptr, BLKSZ);
	resume:
	// Present the data to the controller.
	__asm__ __volatile__ (
		"ldst %%sr, %0"
		:: "r" (addr+HWDRVBLKDEV_SWAP)
		: "memory");
	// Initiate the block write.
	__asm__ __volatile__ (
		"ldst %0, %1"
		: "+r" ((unsigned long){idx})
		: "r" (addr+HWDRVBLKDEV_WRITE)
		: "memory");
	if (nxt) {
		ptr += BLKSZ;
		// Fill controller up with next data to write.
		memcpy (addr, ptr, BLKSZ);
		hwdrvblkdev_write_ptr_saved = ptr;
		hwdrvblkdev_write_idx_saved = (idx + 1);
	} else {
		hwdrvblkdev_write_ptr_saved = (void *)-1;
		hwdrvblkdev_write_idx_saved = -1;
	}
}

// Copy blocks within the block device without the need
// to read() and write() using a buffer.
// The block index of the destination and source are respectively
// given by the arguments dstidx and srcidx, while the count of blocks
// to copy is given by the argument cnt. The fact that the destination
// and source may overlap is taken into account.
// Returns the count of blocks that could be copied.
static unsigned long hwdrvblkdev_cpy (hwdrvblkdev *dev, unsigned long dstidx, unsigned long srcidx, unsigned long cnt) {
	if (!cnt)
		return 0;
	hwdrvblkdev_read_ptr_saved = (void *)-1;
	hwdrvblkdev_read_idx_saved = -1;
	hwdrvblkdev_write_ptr_saved = (void *)-1;
	hwdrvblkdev_write_idx_saved = -1;
	// Variable used to determine whether to copy blocks from
	// the top or bottom to avoid overwriting data when the
	// destination and source overlap.
	unsigned long x = (dstidx > srcidx);
	unsigned long ret = 0;
	if (x) {
		dstidx += cnt;
		srcidx += cnt;
	}
	void* addr = dev->addr;
	do {
		// Initiate the block read.
		__asm__ __volatile__ (
			"ldst %0, %1"
			: "+r" ((unsigned long){x?--srcidx:srcidx++})
			: "r" (addr+HWDRVBLKDEV_READ)
			: "memory");
		// Read status until ready is returned.
		signed long isrdy;
		do {
			if ((isrdy = hwdrvblkdev_isrdy (dev)) < 0)
				return 0;
		} while (!isrdy);
		// Initiate the block write.
		__asm__ __volatile__ (
			"ldst %0, %1"
			: "+r" ((unsigned long){x?--dstidx:dstidx++})
			: "r" (addr+HWDRVBLKDEV_WRITE)
			: "memory");
		// Read status until ready is returned.
		do {
			if ((isrdy = hwdrvblkdev_isrdy (dev)) < 0)
				return 0;
		} while (!isrdy);
		++ret;
	} while (--cnt);
	return ret;
}

#endif /* HWDRVBLKDEV_H */
