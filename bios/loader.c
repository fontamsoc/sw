// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

// The loader must fit in the MBR first 446 bytes.
// It implements only what is necessary to retrieve
// the next stage which is the BIOS.

#include <stdint.h>

// Used to stringify.
#define __xstr__(s) __str__(s)
#define __str__(s) #s

static unsigned char stack[STACKSZ] __attribute__((used));

// Substitute for crt0.S since this is built using -nostdlib.
__asm__ (
	".section .text._start\n"
	".global  _start\n"
	".type    _start, @function\n"
	".p2align 1\n"
	"_start:\n"

	// Adjust %ksl to enable caching throughout the memory
	// region where the loader and BIOS will be running.
	"li %sr, ("__xstr__(KSLVAL)")\n"
	"setksl %sr\n"
	// Initialize %sp and %fp.
	"rli16 %sp, stack + "__xstr__(STACKSZ)"\n"
	//"li8 %fp, 0\n" // ### Disabled, as it is unnecessary.
	// Call main().
	"rli16 %sr, main\n"
	"jl %rp, %sr\n"
	// We should never return from above jl,
	// otherwise we must infinite loop.
	//"j %rp\n" // ### Disabled, as main() will never return.

	".size    _start, (. - _start)\n");

// Copies cnt uints from memory area src to memory area dst.
// The memory areas must not overlap.
// Returns (dst+(cnt*sizeof(unsigned long))).
void *uintcpy (void *dst, void *src, unsigned long cnt); __asm__ (
	".section .text\n"
	".global  uintcpy\n"
	".type    uintcpy, @function\n"
	".p2align 1\n"
	"uintcpy:\n"

	"rli8 %sr, 1f\n"
	"jz %3, %sr\n"
	"rli8 %sr, 0f\n"
	"0: ld %4, %2; st %4, %1\n"
	"inc8 %1, "__xstr__(__SIZEOF_POINTER__)"\n"
	"inc8 %2, "__xstr__(__SIZEOF_POINTER__)"\n"
	"inc8 %3, -1\n"
	"jnz %3, %sr\n"
	"1: j %rp\n"

	".size    uintcpy, (. - uintcpy)\n");

// Block device commands.
#define BLKDEV_RESET	0
#define BLKDEV_SWAP	1
#define BLKDEV_READ	2
#define BLKDEV_WRITE	3
// Read the block at idx and present it at BLKDEVADDR.
// Note that BLKDEV_READ_SIZE in linker script is affected by BLKDEVADDR.
// blkdev_read() must be identical in all chained loaders.
void blkdev_read (unsigned long idx); __asm__ (
	".section .blkdev_read, \"ax\"\n"
	".global  blkdev_read\n"
	".type    blkdev_read, @function\n"
	".p2align 1\n"
	"blkdev_read:\n"

	// Load block with index in %1.
	"li8 %2, "__xstr__(BLKDEV_READ)"*"__xstr__(__SIZEOF_POINTER__)"\n"
	#if BLKDEVADDR != 0
	"inc %2, "__xstr__(BLKDEVADDR)"\n"
	#endif
	"ldst %1, %2\n" // Initiate the block loading.
	// Wait for block load.
	"li8 %2, "__xstr__(BLKDEV_RESET)"*"__xstr__(__SIZEOF_POINTER__)"\n"
	#if BLKDEVADDR != 0
	"inc %2, "__xstr__(BLKDEVADDR)"\n"
	#endif
	"rli8 %sr, 0f\n"
	"0: li8 %1, 0\n" // Set null to prevent reset when reading status.
	"ldst %1, %2\n" // Read status.
	"inc8 %1, -1\n" // Will set null if status was 1 (READY).
	"jnz %1, %sr\n"
	// Present the loaded block in the physical memory.
	"li8 %2, "__xstr__(BLKDEV_SWAP)"*"__xstr__(__SIZEOF_POINTER__)"\n"
	#if BLKDEVADDR != 0
	"inc %2, "__xstr__(BLKDEVADDR)"\n"
	#endif
	"ldst %1, %2\n"
	#if BLKDEVADDR != 0
	"j %rp\n"
	#else /* for case where return address is 0, since j %rp is encoded as jnz %rp, %rp */
	"jnz %sp, %rp\n"
	#endif
	".p2align 3\n" // Aligned in such a way that it works also for __SIZEOF_POINTER__ == 8 .

	".size    blkdev_read, (. - blkdev_read)\n");

// Structure describing the MBR.
typedef struct __attribute__((packed)) {
	unsigned char bootcode[446];
	struct __attribute__((packed)) {
		uint32_t chs_begin :24;
		uint32_t attr :8;
		uint32_t chs_end :24;
		uint32_t type :8;
		uint32_t lba_begin;
		uint32_t sect_cnt;
	} partition_entry[4];
} mbr_t;

extern void* _end;

#define BLKSZ 512 /* block size in bytes */

unsigned long saved_sp __attribute__((used));

__attribute__((noreturn)) void main (void) {
	unsigned v;
	#define LDRMEMINIT
	#ifdef LDRMEMINIT
	// Initialize and test memory.
	unsigned x = 3 /* PRELDRADDR */;
	__asm__ __volatile__ (
		"ldst %0, %1"
		: "+r" (x)
		: "r"  (DEVTBLADDR));
	if (x) {
		__asm__ __volatile__ (
			"rli16 %%sr, saved_sp\n"
			"st %%sp, %%sr\n"
			::: "memory");
		((void(*)(void))x)();
		__asm__ __volatile__ (
			"dcacherst; icacherst\n"
			"rli16 %%sr, saved_sp\n"
			"ld %%sp, %%sr\n"
			::: "memory");
		__asm__ __volatile__ (
			"li8 %%sr, 3 /* RRESET */; ldst %%sr, %0"
			:: "r" (DEVTBLADDR+sizeof(unsigned long)));
		x = 1 /* RAMCACHESZ */;
		__asm__ __volatile__ (
			"ldst %0, %1"
			: "+r" (x)
			: "r"  (DEVTBLADDR));
		x *= (4 /* Test more than the RAM cache to guaranty actual RAM access */ * sizeof(unsigned long));
		for (v = 0; v < x; v += sizeof(unsigned long)) {
			unsigned w = (KERNELADDR + v);
			unsigned u = (/*(~v)^*/w);
			*(unsigned long *)w = u;
		}
		for (v = 0; v < x; v += sizeof(unsigned long)) {
			unsigned w = (KERNELADDR + v);
			unsigned u = (/*(~v)^*/w);
			__asm__ __volatile__ (
				"ldst %0, %1"
				: "+r" (w)
				: "r"  (w));
			if (w != u) {
				__asm__ __volatile__ (
					"li8 %%sr, 2 /* CRESET */; ldst %%sr, %0; rli8 %%sr, 0f; 0: j %%sr"
					:: "r" (DEVTBLADDR+sizeof(unsigned long)));
			}
		}
	}
	#endif /* LDRMEMINIT */
	// Relocate ourself.
	uintcpy (
		(void *)KERNELADDR,
		(void *)BLKDEVADDR,
		(((unsigned long)&_end-(unsigned long)BLKDEVADDR)/sizeof(unsigned long)));
	// Jump to location where we relocated ourself.
	__asm__ __volatile__ (
		"rli8 %%sr, 0f\n"
		"add %%sr, %0; add %%sp, %0; add %%rp, %0\n"
		"j %%sr\n"
		"0:\n" :: "r"((unsigned long)KERNELADDR - (unsigned long)BLKDEVADDR));
	// Read MBR.
	blkdev_read(0);
	unsigned long bios_lba_begin = ((mbr_t *)(BLKDEVADDR))->partition_entry[BIOSPART].lba_begin;
	unsigned long bios_sect_cnt = ((mbr_t *)(BLKDEVADDR))->partition_entry[BIOSPART].sect_cnt;
	unsigned long bios_lba_end = bios_lba_begin + bios_sect_cnt -1;
	// Load BIOS.
	for (v = BIOSADDR; bios_lba_begin <= bios_lba_end; ++bios_lba_begin) {
		blkdev_read(bios_lba_begin);
		v = (unsigned long)uintcpy ((void *)v, BLKDEVADDR, BLKSZ/sizeof(unsigned long));
	}
	// Jump to loaded BIOS.
	goto *(void *)BIOSADDR;
}
