// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

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

	// Initialize %sp and %fp.
	"rli %sp, stack + "__xstr__(STACKSZ)"\n"
	//"li8 %fp, 0\n" // ### Disabled, as it is unnecessary.
	#if 0 // ### Disabled, as section .bss is loaded in memory having been already zeroed.
	// Zero section .bss .
	"rli %8, __bss_start\n"
	"rli %9, __bss_end\n"
	"li8 %10, 0\n"
	"rli %11, 0f\n"
	"rli %12, 1f\n"
	"0: cpy %13, %8\n"
	"sltu %13, %9\n"
	"jz %13, %12\n"
	"st %10, %8\n" // Write zero.
	"inc8 %8, "__xstr__(__SIZEOF_POINTER__)"\n"
	"j %11; 1:\n"
	#endif
	// Call main().
	"rli %sr, main\n"
	"jl %rp, %sr\n"
	// We should never return from above jl,
	// otherwise we must infinite loop.
	//"j %rp\n" // ### Disabled, as main() will never return.

	".size    _start, (. - _start)\n");

// Default kernel argument.
// Its size must be <= KERNELARG_SIZE defined in bios.lds .
__asm__ (
	".section .kernelarg\n"
	".string \"root=/dev/hda4 earlyprintk=keep\"\n");

// 24 is used such that uintcpy() properly copies parkpu() when __SIZEOF_POINTER__ == 8 .
#define PARKPUSZ 24

// multipu expects to find parkpu at (KERNELADDR - PARKPUSZ) where it should have
// been installed; kernel expects to find rli16 immediate at (KERNELADDR - (PARKPUSZ - 14)).
__attribute__((noreturn)) void parkpu (void); __asm__ (
	".section .text\n"
	".global  parkpu\n"
	".type    parkpu, @function\n"
	// .p2align > 1 so that uintcpy() properly copies it.
	#if __SIZEOF_POINTER__ == 8
	".p2align 3\n"
	#else
	".p2align 2\n"
	#endif
	"parkpu:\n"
	"li16 %sr, 0x2000\n"
	"setflags %sr\n"
	"0: halt\n"
	"sysret\n"
	"icacherst\n"
	"rli16 %sr, 0b\n" // Immediate must be 16bits; it aligns along instructions and can be modified with a single st16.
	"j %sr\n"
	// .p2align > 1 so that uintcpy() properly copies it.
	#if __SIZEOF_POINTER__ == 8
	".p2align 3\n"
	#else
	".p2align 2\n"
	#endif
	".global  parkpu_end\n"
	"parkpu_end:\n"
	".p2align 3\n" // Makes sizeof parkpu() match PARKPUSZ.

	".size    parkpu, (. - parkpu)\n");
extern void *parkpu_end;

// Copies cnt uints from memory area src to memory area dst.
// The memory areas must not overlap.
// Returns (dst+(cnt*sizeof(unsigned long))).
void *uintcpy (void *dst, const void *src, unsigned long cnt); __asm__ (
	".text\n"
	".global  uintcpy\n"
	".type    uintcpy, @function\n"
	".p2align 1\n"
	"uintcpy:\n"

	"rli %sr, 1f\n"
	"jz %3, %sr\n"
	"rli %sr, 0f\n"
	"0: ld %4, %2; st %4, %1\n"
	"inc8 %1, "__xstr__(__SIZEOF_POINTER__)"\n"
	"inc8 %2, "__xstr__(__SIZEOF_POINTER__)"\n"
	"inc8 %3, -1\n"
	"jnz %3, %sr\n"
	"1: j %rp\n"

	".size    uintcpy, (. - uintcpy)\n");

// Copies cnt u8s from memory area src to memory area dst.
// The memory areas must not overlap.
// Returns (dst+cnt).
void *u8cpy (void *dst, const void *src, unsigned long cnt); __asm__ (
	".text\n"
	".global  u8cpy\n"
	".type    u8cpy, @function\n"
	".p2align 1\n"
	"u8cpy:\n"

	"rli %sr, 1f\n"
	"jz %3, %sr\n"
	"rli %sr, 0f\n"
	"0: ld8 %4, %2; st8 %4, %1\n"
	"inc8 %1, 1\n"
	"inc8 %2, 1\n"
	"inc8 %3, -1\n"
	"jnz %3, %sr\n"
	"1: j %rp\n"

	".size    u8cpy, (. - u8cpy)\n");

typedef unsigned long size_t;

void *memcpy (void *dest, const void *src, size_t count) {
	if (((unsigned long)dest|(unsigned long)src)%sizeof(unsigned long))
		u8cpy (dest, src, count);
	else
		uintcpy (dest, src, (count/sizeof(unsigned long)));
	return dest;
}

// Structure describing the MBR.
struct __attribute__((packed)) {
	unsigned char bootcode[446];
	struct __attribute__((packed)) {
		uint32_t chs_begin :24;
		uint32_t attr :8;
		uint32_t chs_end :24;
		uint32_t type :8;
		uint32_t lba_begin;
		uint32_t sect_cnt;
	} partition_entry[4];
} *mbr = (void *)BLKDEVADDR;

#include <hwdrvblkdev/hwdrvblkdev.h>
hwdrvblkdev hwdrvblkdev_dev = {.addr = (void *)BLKDEVADDR};

#include <hwdrvchar/hwdrvchar.h>
hwdrvchar hwdrvchar_dev = {.addr = (void *)UARTADDR};

#define BLKSZ 512 /* block size in bytes */

//#include <hwdrvdevtbl/hwdrvdevtbl.h>
//hwdrvdevtbl hwdrvdevtbl_dev = {.e = (devtblentry *)0, .id = 1 /* RAM device */};

#include "version.h"

int putchar (int c) {
	while (!hwdrvchar_write_(&hwdrvchar_dev, &c, 1));
	return c;
}

#include <print/print.h>

//#define DO_KERNEL_HEXDUMP
#ifdef DO_KERNEL_HEXDUMP
// Compare cnt uints between memory areas src and dst.
// When equal, it returns (dst+(cnt*sizeof(unsigned long))).
void *uintcmp (void *dst, void *src, unsigned long cnt); __asm__ (
	".text\n"
	".global  uintcmp\n"
	".type    uintcmp, @function\n"
	".p2align 1\n"
	"uintcmp:\n"

	"rli %sr, 1f\n"
	"jz %3, %sr\n"
	"rli %sr, 0f\n"
	"rli %6, 1f\n"
	"0: ld %4, %2; ld %5, %1\n"
	"seq %4, %5; jz %4, %6\n"
	"inc8 %1, "__xstr__(__SIZEOF_POINTER__)"\n"
	"inc8 %2, "__xstr__(__SIZEOF_POINTER__)"\n"
	"inc8 %3, -1\n"
	"jnz %3, %sr\n"
	"1: j %rp\n"

	".size    uintcmp, (. - uintcmp)\n");

#include <hexdump/hexdump.h>
#endif

__attribute__((noreturn)) void main (void) {

	hwdrvchar_init (&hwdrvchar_dev, UARTBAUD);

	unsigned long socversion = 0;
	__asm__ __volatile__ (
		"ldst %0, %1"
		: "+r" (socversion)
		: "r"  (DEVTBLADDR));
	printstr("\nsoc  "); printhex(socversion); printstr("\n");

	printstr(BIOSVERSION);

	// Initialize %ksysopfaulthdlr.
	__asm__ __volatile__ (
		"rli %sr, ksysopfaulthdlr\n"
		"setksysopfaulthdlr %sr\n");
	// Save ksysopfaulthdlr address to be retrieved from kernel environment.
	extern void *___ishw;
	void ksysopfaulthdlr (void);
	*(unsigned long *)((void *)&___ishw + 8/*sizeof("___ISHW=")*/) = (unsigned long)ksysopfaulthdlr;

	extern void *__executable_start, *_end;

	// Install parkpu() at the bottom of the bios region.
	unsigned long parkpu_sz = ((unsigned long)&parkpu_end - (unsigned long)&parkpu);
	if (parkpu_sz % sizeof(unsigned long)) { // parkpu() size must be appropriate for uintcpy().
		//printstr("parkpu() has invalid size\n"); ###: Commented out to reduce BIOS size.
		parkpu();
	}
	unsigned long parkpu_addr = (KERNELADDR - PARKPUSZ);
	if ((unsigned long)&_end > parkpu_addr) {
		//printstr("parkpu() cannot be installed\n"); ###: Commented out to reduce BIOS size.
		parkpu();
	}
	uintcpy ((void *)parkpu_addr, &parkpu, parkpu_sz/sizeof(unsigned long));

	if (!hwdrvblkdev_init (&hwdrvblkdev_dev, 0)) {
		printstr("blkdev initialization failed\n");
		parkpu();
	}

	// Retrieve the kernel location from the MBR.
	unsigned long kernel_lba_begin = mbr->partition_entry[KERNPART].lba_begin;
	unsigned long kernel_sect_cnt = mbr->partition_entry[KERNPART].sect_cnt;
	unsigned long kernel_lba_end = kernel_lba_begin + kernel_sect_cnt -1;

	// Look for RAM device.
	/*while (hwdrvdevtbl_find (&hwdrvdevtbl_dev),
		(hwdrvdevtbl_dev.mapsz && hwdrvdevtbl_dev.addr <= (void *)KERNELADDR)) {
		if ((hwdrvdevtbl_dev.addr + (hwdrvdevtbl_dev.mapsz*sizeof(unsigned long))) >=
			((void *)KERNELADDR + (kernel_sect_cnt*BLKSZ)))
			break;
	}
	if (!hwdrvdevtbl_dev.mapsz || hwdrvdevtbl_dev.addr > (void *)KERNELADDR) {
		printstr("no ram device large enough for kernel\n");
		parkpu();
	}*/

	// Adjust %ksl to enable caching throughout the memory region where the kernel is to be loaded.
	asm volatile ("setksl %0\n" :: "r"(KERNELADDR+(((kernel_lba_end-kernel_lba_begin)+1)*BLKSZ)));

	// Load kernel.
	void *k = (void *)KERNELADDR;
	for (unsigned long i = kernel_lba_begin; i <= kernel_lba_end;) {
		signed long isrdy = hwdrvblkdev_isrdy (&hwdrvblkdev_dev);
		if (isrdy < 0) {
			printstr("blkdev read error\n");
			parkpu();
		}
		if (isrdy == 0)
			continue;
		unsigned long n = hwdrvblkdev_read (&hwdrvblkdev_dev, (void *)k, i, ((i + 1) <= kernel_lba_end));
		k += (n*BLKSZ);
		i += n;
	}

	printstr("kernel loaded\n");

	#ifdef DO_KERNEL_HEXDUMP
	hexdump ((void *)KERNELADDR, kernel_sect_cnt*BLKSZ);
	#endif

	// Setup the initial kernel stack as follow:
	// - argc
	// - null-terminated argv pointers array.
	// - null-terminated envp pointers array.

	volatile unsigned long p[6]; // Declared volatile so that GCC does not optimize it out.

	p[0] = 2;
	extern void *kernelarg_start;
	p[1] = (unsigned long)"";
	p[2] = (unsigned long)&kernelarg_start;
	p[3] = 0;
	extern void *___ishw;
	p[4] = (unsigned long)&___ishw;
	p[5] = 0;

	__asm__ __volatile__ (
		"cpy %%sp, %0\n"
		"dcacherst\n"
		"icacherst\n"
		"jl %%rp, %1\n"
		:: "r"(p), "r"(KERNELADDR));

	parkpu();
}

/* void ksysopfaulthdlr (void) */ __asm__ (
	".data\n"
	".align "__xstr__(__SIZEOF_POINTER__)"\n"
	".type ___ishw, @object\n"
	"___ishw: .ascii \"___ISHW=________\"\n"
	".size    ___ishw, (. - ___ishw)\n"

	".text\n"
	".global  ksysopfaulthdlr\n"
	".type    ksysopfaulthdlr, @function\n"
	".p2align 1\n"
	"ksysopfaulthdlr:\n"

	/* Must be exactely two 16bits instructions
	   as ksysopfault from kernelmode will execute from
	   (%ksysopfaulthdlr + 4), while from usermode
	   will execute from %ksysopfaulthdlr. */
	"rli8 %sr, 0f; j %sr\n"

	"inc8 %sp, -"__xstr__(2*__SIZEOF_POINTER__)"; st %1, %sp\n"
	"inc8 %sp, -"__xstr__(__SIZEOF_POINTER__)"; st %2, %sp\n"
	"inc8 %sp, -"__xstr__(__SIZEOF_POINTER__)"; st %3, %sp\n"
	"inc8 %sp, -"__xstr__(__SIZEOF_POINTER__)"; st %4, %sp\n"
	"inc8 %sp, -"__xstr__(__SIZEOF_POINTER__)"; st %5, %sp\n"
	"inc8 %sp, -"__xstr__(__SIZEOF_POINTER__)"; st %6, %sp\n"
	"inc8 %sp, -"__xstr__(__SIZEOF_POINTER__)"; st %7, %sp\n"
	"inc8 %sp, -"__xstr__(__SIZEOF_POINTER__)"; st %8, %sp\n"
	"inc8 %sp, -"__xstr__(__SIZEOF_POINTER__)"; st %9, %sp\n"
	"inc8 %sp, -"__xstr__(__SIZEOF_POINTER__)"; st %10, %sp\n"
	"inc8 %sp, -"__xstr__(__SIZEOF_POINTER__)"; st %11, %sp\n"
	"inc8 %sp, -"__xstr__(__SIZEOF_POINTER__)"; st %12, %sp\n"
	"inc8 %sp, -"__xstr__(__SIZEOF_POINTER__)"; st %13, %sp\n"
	"inc8 %sp, -"__xstr__(__SIZEOF_POINTER__)"; st %14, %sp\n"
	"inc8 %sp, -"__xstr__(__SIZEOF_POINTER__)"; st %15, %sp\n"
	// Save the initial stack pointer value.
	"cpy %2, %sp; inc8 %2, "__xstr__(15*__SIZEOF_POINTER__)"\n"
	"cpy %1, %2; inc8 %1, "__xstr__(__SIZEOF_POINTER__)"\n"
	"st %1, %2\n"

	"cpy %1, %sp; rli %sr, 1f; j %sr\n"
	"0:\n" /* we branch here to skip saving registers */
	"li %1, 0\n"
	"1:\n" /* we branch here when registers were saved */

	// Call instruction handlers based on the value of %sysopcode.
	// void opcodehdlr (savedkctx *kctx, unsigned long opcode);
	// When argument kctx is null, ksysopfault occured in usermode.
	"getsysopcode %2\n"
	"li %3, 0x01; seq %3, %2; rli %sr, 0f; jz %3, %sr; rli %rp, 1f; rli %sr, syscallhdlr; j %sr; 0:\n"
	"li %3, 0xfc; cpy %4, %3; and %4, %2; seq %3, %4; rli %sr, 0f; jz %3, %sr; rli %rp, 1f; rli %sr, cldsthdlr; j %sr; 0:\n"
	"li %3, 0xd8; cpy %4, %3; and %4, %2; seq %3, %4; rli %sr, 0f; jz %3, %sr; rli %rp, 1f; rli %sr, floathdlr; j %sr; 0:\n"
	"rli %sr, badopcode; jl %rp, %sr\n"

	"1: rli %sr, 0f; jz %1, %sr\n"

	"ld %15, %sp; inc8 %sp, "__xstr__(__SIZEOF_POINTER__)"\n"
	"ld %14, %sp; inc8 %sp, "__xstr__(__SIZEOF_POINTER__)"\n"
	"ld %13, %sp; inc8 %sp, "__xstr__(__SIZEOF_POINTER__)"\n"
	"ld %12, %sp; inc8 %sp, "__xstr__(__SIZEOF_POINTER__)"\n"
	"ld %11, %sp; inc8 %sp, "__xstr__(__SIZEOF_POINTER__)"\n"
	"ld %10, %sp; inc8 %sp, "__xstr__(__SIZEOF_POINTER__)"\n"
	"ld %9, %sp; inc8 %sp, "__xstr__(__SIZEOF_POINTER__)"\n"
	"ld %8, %sp; inc8 %sp, "__xstr__(__SIZEOF_POINTER__)"\n"
	"ld %7, %sp; inc8 %sp, "__xstr__(__SIZEOF_POINTER__)"\n"
	"ld %6, %sp; inc8 %sp, "__xstr__(__SIZEOF_POINTER__)"\n"
	"ld %5, %sp; inc8 %sp, "__xstr__(__SIZEOF_POINTER__)"\n"
	"ld %4, %sp; inc8 %sp, "__xstr__(__SIZEOF_POINTER__)"\n"
	"ld %3, %sp; inc8 %sp, "__xstr__(__SIZEOF_POINTER__)"\n"
	"ld %2, %sp; inc8 %sp, "__xstr__(__SIZEOF_POINTER__)"\n"
	"ld %1, %sp; inc8 %sp, "__xstr__(__SIZEOF_POINTER__)"\n"
	// Restoring %sp last in case the handler function gave it a new value.
	"ld %sp, %sp\n"

	"0: ksysret\n"

	".size    ksysopfaulthdlr, (. - ksysopfaulthdlr)\n");

typedef union {
	struct {
		unsigned long r15;
		unsigned long r14;
		unsigned long r13;
		unsigned long r12;
		unsigned long r11;
		unsigned long r10;
		unsigned long r9;
		unsigned long r8;
		unsigned long r7;
		unsigned long r6;
		unsigned long r5;
		unsigned long r4;
		unsigned long r3;
		unsigned long r2;
		unsigned long r1;
		unsigned long r0;
	};
	unsigned long r[16];
} savedkctx;

savedkctx * badopcode (savedkctx *kctx, unsigned long opcode) {
	printstr("badopcode: "); printu8hex(opcode); putchar(' '); printu8hex(opcode>>8); putchar('\n');
	parkpu();
	return kctx;
}

#include <mutex/mutex.h>

savedkctx * cldsthdlr (savedkctx *kctx, unsigned long opcode) {

	unsigned long gpr1 = ((opcode & 0xf000) >> 12), gpr2;
	unsigned long srval, gpr1val, gpr2val;

	if (kctx) {
		srval = kctx->r13;
		gpr1val = kctx->r[15-gpr1];
		gpr2 = ((opcode & 0x0f00) >> 8);
		gpr2val = kctx->r[15-gpr2];
	} else {
		__asm__ __volatile__ ("setkgpr %0, %%sr\n" : "=r"(srval));
		switch (gpr1) {
			case 0: __asm__ __volatile__ ("setkgpr %0, %%0\n" : "=r"(gpr1val)); break;
			case 1: __asm__ __volatile__ ("setkgpr %0, %%1\n" : "=r"(gpr1val)); break;
			case 2: __asm__ __volatile__ ("setkgpr %0, %%2\n" : "=r"(gpr1val)); break;
			case 3: __asm__ __volatile__ ("setkgpr %0, %%3\n" : "=r"(gpr1val)); break;
			case 4: __asm__ __volatile__ ("setkgpr %0, %%4\n" : "=r"(gpr1val)); break;
			case 5: __asm__ __volatile__ ("setkgpr %0, %%5\n" : "=r"(gpr1val)); break;
			case 6: __asm__ __volatile__ ("setkgpr %0, %%6\n" : "=r"(gpr1val)); break;
			case 7: __asm__ __volatile__ ("setkgpr %0, %%7\n" : "=r"(gpr1val)); break;
			case 8: __asm__ __volatile__ ("setkgpr %0, %%8\n" : "=r"(gpr1val)); break;
			case 9: __asm__ __volatile__ ("setkgpr %0, %%9\n" : "=r"(gpr1val)); break;
			case 10: __asm__ __volatile__ ("setkgpr %0, %%10\n" : "=r"(gpr1val)); break;
			case 11: __asm__ __volatile__ ("setkgpr %0, %%11\n" : "=r"(gpr1val)); break;
			case 12: __asm__ __volatile__ ("setkgpr %0, %%12\n" : "=r"(gpr1val)); break;
			case 13: __asm__ __volatile__ ("setkgpr %0, %%13\n" : "=r"(gpr1val)); break;
			case 14: __asm__ __volatile__ ("setkgpr %0, %%14\n" : "=r"(gpr1val)); break;
			case 15: __asm__ __volatile__ ("setkgpr %0, %%15\n" : "=r"(gpr1val)); break;
		}
		__asm__ __volatile__ ("getfaultaddr %0\n" : "=r"(gpr2val));
	}

	static mutex cldstmutexarray[CLDSTMUTEXCNT] = {[0 ... CLDSTMUTEXCNT - 1] = {0, 0, 0}};

	switch ((opcode & 0xff)) {
		case 0xfc: { // cldst8
			mutex *m = &cldstmutexarray[(gpr2val/sizeof(unsigned long))%(sizeof(cldstmutexarray)/sizeof(mutex))];
			mutex_lock (m);
			uint8_t old_gpr2val = *(volatile uint8_t *)gpr2val;
			if (old_gpr2val == srval)
				*(volatile uint8_t *)gpr2val = gpr1val;
			mutex_unlock (m);
			gpr1val = old_gpr2val;
			break; }
		case 0xfd: { // cldst16
			mutex *m = &cldstmutexarray[(gpr2val/sizeof(unsigned long))%(sizeof(cldstmutexarray)/sizeof(mutex))];
			mutex_lock (m);
			uint16_t old_gpr2val = *(volatile uint16_t *)gpr2val;
			if (old_gpr2val == srval)
				*(volatile uint16_t *)gpr2val = gpr1val;
			mutex_unlock (m);
			gpr1val = old_gpr2val;
			break; }
		#if __SIZEOF_POINTER__ >= 4
		case 0xfe: { // cldst32
			mutex *m = &cldstmutexarray[(gpr2val/sizeof(unsigned long))%(sizeof(cldstmutexarray)/sizeof(mutex))];
			mutex_lock (m);
			uint32_t old_gpr2val = *(volatile uint32_t *)gpr2val;
			if (old_gpr2val == srval)
				*(volatile uint32_t *)gpr2val = gpr1val;
			mutex_unlock (m);
			gpr1val = old_gpr2val;
			break; }
		#endif
		#if __SIZEOF_POINTER__ >= 8
		case 0xff: { // cldst64
			mutex *m = &cldstmutexarray[(gpr2val/sizeof(unsigned long))%(sizeof(cldstmutexarray)/sizeof(mutex))];
			mutex_lock (m);
			uint64_t old_gpr2val = *(volatile uint64_t *)gpr2val;
			if (old_gpr2val == srval)
				*(volatile uint64_t *)gpr2val = gpr1val;
			mutex_unlock (m);
			gpr1val = old_gpr2val;
			break; }
		#endif
	}

	if (kctx)
		kctx->r[15-gpr1] = gpr1val;
	else switch (gpr1) {
		case 0: __asm__ __volatile__ ("setugpr %%0, %0\n" :: "r"(gpr1val)); break;
		case 1: __asm__ __volatile__ ("setugpr %%1, %0\n" :: "r"(gpr1val)); break;
		case 2: __asm__ __volatile__ ("setugpr %%2, %0\n" :: "r"(gpr1val)); break;
		case 3: __asm__ __volatile__ ("setugpr %%3, %0\n" :: "r"(gpr1val)); break;
		case 4: __asm__ __volatile__ ("setugpr %%4, %0\n" :: "r"(gpr1val)); break;
		case 5: __asm__ __volatile__ ("setugpr %%5, %0\n" :: "r"(gpr1val)); break;
		case 6: __asm__ __volatile__ ("setugpr %%6, %0\n" :: "r"(gpr1val)); break;
		case 7: __asm__ __volatile__ ("setugpr %%7, %0\n" :: "r"(gpr1val)); break;
		case 8: __asm__ __volatile__ ("setugpr %%8, %0\n" :: "r"(gpr1val)); break;
		case 9: __asm__ __volatile__ ("setugpr %%9, %0\n" :: "r"(gpr1val)); break;
		case 10: __asm__ __volatile__ ("setugpr %%10, %0\n" :: "r"(gpr1val)); break;
		case 11: __asm__ __volatile__ ("setugpr %%11, %0\n" :: "r"(gpr1val)); break;
		case 12: __asm__ __volatile__ ("setugpr %%12, %0\n" :: "r"(gpr1val)); break;
		case 13: __asm__ __volatile__ ("setugpr %%13, %0\n" :: "r"(gpr1val)); break;
		case 14: __asm__ __volatile__ ("setugpr %%14, %0\n" :: "r"(gpr1val)); break;
		case 15: __asm__ __volatile__ ("setugpr %%15, %0\n" :: "r"(gpr1val)); break;
	}

	return kctx;
}

savedkctx * floathdlr (savedkctx *kctx, unsigned long opcode) {

	unsigned long gpr1 = ((opcode & 0xf000) >> 12);
	unsigned long gpr2 = ((opcode & 0x0f00) >> 8);

	union {
		unsigned long i;
		float f;
	} gpr1val, gpr2val;

	if (kctx) {
		gpr1val.i = kctx->r[15-gpr1];
		gpr2val.i = kctx->r[15-gpr2];
	} else {
		switch (gpr1) {
			case 0: __asm__ __volatile__ ("setkgpr %0, %%0\n" : "=r"(gpr1val.i)); break;
			case 1: __asm__ __volatile__ ("setkgpr %0, %%1\n" : "=r"(gpr1val.i)); break;
			case 2: __asm__ __volatile__ ("setkgpr %0, %%2\n" : "=r"(gpr1val.i)); break;
			case 3: __asm__ __volatile__ ("setkgpr %0, %%3\n" : "=r"(gpr1val.i)); break;
			case 4: __asm__ __volatile__ ("setkgpr %0, %%4\n" : "=r"(gpr1val.i)); break;
			case 5: __asm__ __volatile__ ("setkgpr %0, %%5\n" : "=r"(gpr1val.i)); break;
			case 6: __asm__ __volatile__ ("setkgpr %0, %%6\n" : "=r"(gpr1val.i)); break;
			case 7: __asm__ __volatile__ ("setkgpr %0, %%7\n" : "=r"(gpr1val.i)); break;
			case 8: __asm__ __volatile__ ("setkgpr %0, %%8\n" : "=r"(gpr1val.i)); break;
			case 9: __asm__ __volatile__ ("setkgpr %0, %%9\n" : "=r"(gpr1val.i)); break;
			case 10: __asm__ __volatile__ ("setkgpr %0, %%10\n" : "=r"(gpr1val.i)); break;
			case 11: __asm__ __volatile__ ("setkgpr %0, %%11\n" : "=r"(gpr1val.i)); break;
			case 12: __asm__ __volatile__ ("setkgpr %0, %%12\n" : "=r"(gpr1val.i)); break;
			case 13: __asm__ __volatile__ ("setkgpr %0, %%13\n" : "=r"(gpr1val.i)); break;
			case 14: __asm__ __volatile__ ("setkgpr %0, %%14\n" : "=r"(gpr1val.i)); break;
			case 15: __asm__ __volatile__ ("setkgpr %0, %%15\n" : "=r"(gpr1val.i)); break;
		}
		switch (gpr2) {
			case 0: __asm__ __volatile__ ("setkgpr %0, %%0\n" : "=r"(gpr2val.i)); break;
			case 1: __asm__ __volatile__ ("setkgpr %0, %%1\n" : "=r"(gpr2val.i)); break;
			case 2: __asm__ __volatile__ ("setkgpr %0, %%2\n" : "=r"(gpr2val.i)); break;
			case 3: __asm__ __volatile__ ("setkgpr %0, %%3\n" : "=r"(gpr2val.i)); break;
			case 4: __asm__ __volatile__ ("setkgpr %0, %%4\n" : "=r"(gpr2val.i)); break;
			case 5: __asm__ __volatile__ ("setkgpr %0, %%5\n" : "=r"(gpr2val.i)); break;
			case 6: __asm__ __volatile__ ("setkgpr %0, %%6\n" : "=r"(gpr2val.i)); break;
			case 7: __asm__ __volatile__ ("setkgpr %0, %%7\n" : "=r"(gpr2val.i)); break;
			case 8: __asm__ __volatile__ ("setkgpr %0, %%8\n" : "=r"(gpr2val.i)); break;
			case 9: __asm__ __volatile__ ("setkgpr %0, %%9\n" : "=r"(gpr2val.i)); break;
			case 10: __asm__ __volatile__ ("setkgpr %0, %%10\n" : "=r"(gpr2val.i)); break;
			case 11: __asm__ __volatile__ ("setkgpr %0, %%11\n" : "=r"(gpr2val.i)); break;
			case 12: __asm__ __volatile__ ("setkgpr %0, %%12\n" : "=r"(gpr2val.i)); break;
			case 13: __asm__ __volatile__ ("setkgpr %0, %%13\n" : "=r"(gpr2val.i)); break;
			case 14: __asm__ __volatile__ ("setkgpr %0, %%14\n" : "=r"(gpr2val.i)); break;
			case 15: __asm__ __volatile__ ("setkgpr %0, %%15\n" : "=r"(gpr2val.i)); break;
		}
	}

	switch ((opcode & 0xff)) {
		#if __SIZEOF_POINTER__ == 4
		case 0xd8: { // fadd
			float __addsf3 (float a, float b);
			gpr1val.f = __addsf3 (gpr1val.f, gpr2val.f);
			break; }
		case 0xd9: { // fsub
			float __subsf3 (float a, float b);
			gpr1val.f = __subsf3 (gpr1val.f, gpr2val.f);
			break; }
		case 0xda: { // fmul
			float __mulsf3 (float a, float b);
			gpr1val.f = __mulsf3 (gpr1val.f, gpr2val.f);
			break; }
		case 0xdb: { // fdiv
			float __divsf3 (float a, float b);
			gpr1val.f = __divsf3 (gpr1val.f, gpr2val.f);
			break; }
		#endif
		default: {
			badopcode (kctx, opcode);
			break; }
	}

	if (kctx)
		kctx->r[15-gpr1] = gpr1val.i;
	else switch (gpr1) {
		case 0: __asm__ __volatile__ ("setugpr %%0, %0\n" :: "r"(gpr1val.i)); break;
		case 1: __asm__ __volatile__ ("setugpr %%1, %0\n" :: "r"(gpr1val.i)); break;
		case 2: __asm__ __volatile__ ("setugpr %%2, %0\n" :: "r"(gpr1val.i)); break;
		case 3: __asm__ __volatile__ ("setugpr %%3, %0\n" :: "r"(gpr1val.i)); break;
		case 4: __asm__ __volatile__ ("setugpr %%4, %0\n" :: "r"(gpr1val.i)); break;
		case 5: __asm__ __volatile__ ("setugpr %%5, %0\n" :: "r"(gpr1val.i)); break;
		case 6: __asm__ __volatile__ ("setugpr %%6, %0\n" :: "r"(gpr1val.i)); break;
		case 7: __asm__ __volatile__ ("setugpr %%7, %0\n" :: "r"(gpr1val.i)); break;
		case 8: __asm__ __volatile__ ("setugpr %%8, %0\n" :: "r"(gpr1val.i)); break;
		case 9: __asm__ __volatile__ ("setugpr %%9, %0\n" :: "r"(gpr1val.i)); break;
		case 10: __asm__ __volatile__ ("setugpr %%10, %0\n" :: "r"(gpr1val.i)); break;
		case 11: __asm__ __volatile__ ("setugpr %%11, %0\n" :: "r"(gpr1val.i)); break;
		case 12: __asm__ __volatile__ ("setugpr %%12, %0\n" :: "r"(gpr1val.i)); break;
		case 13: __asm__ __volatile__ ("setugpr %%13, %0\n" :: "r"(gpr1val.i)); break;
		case 14: __asm__ __volatile__ ("setugpr %%14, %0\n" :: "r"(gpr1val.i)); break;
		case 15: __asm__ __volatile__ ("setugpr %%15, %0\n" :: "r"(gpr1val.i)); break;
	}

	return kctx;
}

unsigned long hwdrvblkdev_blkoffs[MAXCORECNT] = {
	[0 ... MAXCORECNT - 1] = 0,
};

savedkctx * syscallhdlr (savedkctx *kctx, unsigned long _) {

	unsigned long sr; // %sr: syscall number.
	unsigned long r1; // %r1: arg1.
	unsigned long r2; // %r1: arg2.
	unsigned long r3; // %r1: arg3.

	if (kctx)
		sr = kctx->r13;
	else
		__asm__ __volatile__ ("setkgpr %0, %%sr\n" : "=r"(sr));

	switch (sr) {

		case __NR_lseek: { // off_t lseek (int fd, off_t offset, int whence);

			if (kctx) {
				r1 = kctx->r1;
				r2 = kctx->r2;
				r3 = kctx->r3;
			} else
				__asm__ __volatile__ (
					"setkgpr %0, %%1\n"
					"setkgpr %1, %%2\n"
					"setkgpr %2, %%3\n"
					: "=r"(r1), "=r"(r2), "=r"(r3));

			unsigned long coreid = 0;
			#if (MAXCORECNT > 1)
			asm volatile ("getcoreid %0" : "=r"(coreid));
			#endif

			if (coreid >= MAXCORECNT || r1 != BIOS_FD_STORAGEDEV)
				goto error;

			if (r3 == SEEK_SET) {
				if (r2 < hwdrvblkdev_dev.blkcnt)
					r1 = (hwdrvblkdev_blkoffs[coreid] = r2);
				else
					goto error;
			} else if (r3 == SEEK_CUR) {
				if ((hwdrvblkdev_blkoffs[coreid]+r2) < hwdrvblkdev_dev.blkcnt)
					r1 = (hwdrvblkdev_blkoffs[coreid] += r2);
				else
					goto error;
			} else if (r3 == SEEK_END) {
				r2 = (hwdrvblkdev_dev.blkcnt+r2);
				if (r2 <= hwdrvblkdev_dev.blkcnt)
					r1 = (hwdrvblkdev_blkoffs[coreid] = r2);
				else
					goto error;
			} else
				goto error;

			goto done;

			break;
		}

		case __NR_read: { // ssize_t read (int fd, void *buf, size_t count);

			if (kctx) {
				r1 = kctx->r1;
				r2 = kctx->r2;
				r3 = kctx->r3;
			} else
				__asm__ __volatile__ (
					"setkgpr %0, %%1\n"
					"setkgpr %1, %%2\n"
					"setkgpr %2, %%3\n"
					: "=r"(r1), "=r"(r2), "=r"(r3));

			if (r1 == BIOS_FD_STDIN) {
				#if (MAXCORECNT > 1)
				static mutex m = {0, 0, 0};
				mutex_lock (&m); // Done for multicore support.
				#endif
				r1 = hwdrvchar_read (&hwdrvchar_dev, (unsigned char *)r2, r3);
				#if (MAXCORECNT > 1)
				mutex_unlock (&m);
				#endif
			} else if (r1 == BIOS_FD_STORAGEDEV) {
				if (r3 == 0) {
					r1 = 0;
					goto done;
				}
				unsigned long coreid = 0;
				#if (MAXCORECNT > 1)
				asm volatile ("getcoreid %0" : "=r"(coreid));
				static mutex m = {0, 0, 0};
				mutex_lock (&m); // Done for multicore support.
				#endif
				if (hwdrvblkdev_blkoffs[coreid] >= hwdrvblkdev_dev.blkcnt)
					goto error;
				signed long isrdy = hwdrvblkdev_isrdy (&hwdrvblkdev_dev);
				if (isrdy < 0 &&
					!hwdrvblkdev_init (&hwdrvblkdev_dev, 0)) {
					//printstr("blkdev initialization failed\n");
					//printstr("blkdev read error\n");
					goto error;
				}
				if (isrdy == 0) {
					r1 = 0;
					goto done;
				}
				hwdrvblkdev_blkoffs[coreid] +=
					(r1 = hwdrvblkdev_read (
						&hwdrvblkdev_dev, (void *)r2, hwdrvblkdev_blkoffs[coreid], (r3 > 1)));
				// Note that return value is not byte amount
				// but number of blocks read.
				#if (MAXCORECNT > 1)
				mutex_unlock (&m);
				#endif
			} else
				goto error;

			goto done;

			break;
		}

		case __NR_write: { // ssize_t write (int fd, const void *buf, size_t count);

			if (kctx) {
				r1 = kctx->r1;
				r2 = kctx->r2;
				r3 = kctx->r3;
			} else
				__asm__ __volatile__ (
					"setkgpr %0, %%1\n"
					"setkgpr %1, %%2\n"
					"setkgpr %2, %%3\n"
					: "=r"(r1), "=r"(r2), "=r"(r3));

			if (r1 == BIOS_FD_STDOUT || r1 == BIOS_FD_STDERR) {
				#if (MAXCORECNT > 1)
				static mutex m = {0, 0, 0};
				mutex_lock (&m); // Done for multicore support.
				#endif
				r1 = hwdrvchar_write_ (&hwdrvchar_dev, (unsigned char *)r2, r3);
				#if (MAXCORECNT > 1)
				mutex_unlock (&m);
				#endif
			} else if (r1 == BIOS_FD_STORAGEDEV) {
				if (r3 == 0) {
					r1 = 0;
					goto done;
				}
				unsigned long coreid = 0;
				#if (MAXCORECNT > 1)
				asm volatile ("getcoreid %0" : "=r"(coreid));
				static mutex m = {0, 0, 0};
				mutex_lock (&m); // Done for multicore support.
				#endif
				if (hwdrvblkdev_blkoffs[coreid] >= hwdrvblkdev_dev.blkcnt)
					goto error;
				signed long isrdy = hwdrvblkdev_isrdy (&hwdrvblkdev_dev);
				if (isrdy < 0 &&
					!hwdrvblkdev_init (&hwdrvblkdev_dev, 0)) {
					//printstr("blkdev initialization failed\n");
					//printstr("blkdev write error\n");
					goto error;
				}
				if (isrdy == 0) {
					r1 = 0;
					goto done;
				}
				hwdrvblkdev_write (&hwdrvblkdev_dev, (void *)r2, hwdrvblkdev_blkoffs[coreid], (r3 > 1));
				hwdrvblkdev_blkoffs[coreid] += 1;
				r1 = 1;
				// Note that return value is not byte amount
				// but number of blocks written.
				#if (MAXCORECNT > 1)
				mutex_unlock (&m);
				#endif
			} else
				goto error;

			goto done;

			break;
		}

		case __NR_exit: { // void exit (int status);

			if (kctx)
				r1 = kctx->r1;
			else
				__asm__ __volatile__ ("setkgpr %0, %%1\n" : "=r"(r1));

			__asm__ __volatile__ (
				"ldst %0, %1"
				: "+r" (r1)
				: "r"  (DEVTBLADDR+sizeof(unsigned long)));

			goto done;

			break;
		}

		default: {

			error:
			r1 = -1;
			done:
			if (kctx)
				kctx->r1 = r1;
			else
				__asm__ __volatile__ ("setugpr %%1, %0\n" :: "r"(r1));
			break;
		}
	}

	return kctx;
}
