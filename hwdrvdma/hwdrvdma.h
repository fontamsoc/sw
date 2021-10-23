// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef HWDRVDMA_H
#define HWDRVDMA_H

// Structure representing a DMA device.
// The field addr must be valid.
typedef struct {
	// Device address.
	void* addr;
	// Count of DMA channels.
	unsigned long chancnt;
} hwdrvdma;

// Commands.
#define HWDRVDMA_CHANSEL	(0*__SIZEOF_POINTER__)
#define HWDRVDMA_SETXFER	(1*__SIZEOF_POINTER__)

// Select channel idx for DMA device at the address given
// through dev->addr; dev->chancnt gets set by this function.
static inline void hwdrvdma_sel (hwdrvdma *dev, unsigned long idx) {
	dev->chancnt = idx;
	__asm__ __volatile__ (
		"ldst %0, %1"
		: "+r" (dev->chancnt)
		: "r" (dev->addr+HWDRVDMA_CHANSEL));
}

// Incremental DMA transfer by cnt bytes from source range [src_start_addr, src_end_addr]
// to destination range [dst_start_addr, dst_end_addr] wrapping around respective ranges.
// The source and destination ranges can overlap only if they have the same size
// and the destination region is at a lower address than the source region.
// Returns the number of bytes left to transfer, where *_addr arguments
// are ignored when the argument cnt is 0 or -1.
// When the argument cnt is 0, an ongoing transfer terminates.
// When the argument cnt is -1, no change is done, and is to be used
// to obtain the byte amount left to transfer without modifications.
static inline unsigned long hwdrvdma_xfer (
	hwdrvdma *dev,
	void *dst_start_addr, void *dst_end_addr,
	void *src_start_addr, void *src_end_addr,
	unsigned long cnt) {
	if (cnt == 0 || cnt == -1) {
		__asm__ __volatile__ (
			"ldst %0, %1"
			: "+r" (cnt)
			: "r" (dev->addr+HWDRVDMA_SETXFER));
		return cnt;
	}
	((void **)dev->addr)[0] = dst_start_addr;
	((void **)dev->addr)[1] = dst_end_addr;
	((void **)dev->addr)[2] = src_start_addr;
	((void **)dev->addr)[3] = src_end_addr;
	__asm__ __volatile__ (
		"ldst %0, %1"
		: "+r" (cnt)
		: "r" (dev->addr+HWDRVDMA_SETXFER));
	return cnt;
}

#endif /* HWDRVDMA_H */
