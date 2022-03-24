// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

// Use as kernel:
// sudo /opt/pu32-toolchain/bin/pu32-mksocimg -k memtest.bin memtest.img

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

	// Disable timer and external interrupts; enable instruction getclkcyclecnt.
	"li %sr, (0x10 | 0x1000 | 0x2000)\n"
	"setflags %sr\n"
	"li %sr, 0x1000\n"
	"setksl %sr\n"

	// Clear all TLB entries.
	"gettlbsize %1\n"
	"li %2, 12\n"
	"sll %1, %2\n"
	"li %3, 0\n"
	"li %2, (1<<12)\n"
	"0: sub %1, %2\n"
	"clrtlb %3, %1\n"
	"rli %sr, 0b\n"
	"jnz %1, %sr\n"
	"li %sr, 0\n" // %sr is expected to have the address of the Page-Global-Directory.
	// Re-using %2 value to set userspace asid.
	"setasid %2\n"

	// Continue execution in usermode.
	"rli %sr, 1f\n"
	"setuip %sr\n"
	"0: sysret\n"

	//#define DEBUG_KERNELMODE

	#ifdef DEBUG_KERNELMODE
	#define HWDRVCHAR_CMDGETBUFFERUSAGE 0
	"80: li %2, "__xstr__(UARTADDR)"\n"
	"li %1, "__xstr__((HWDRVCHAR_CMDGETBUFFERUSAGE<<30) | 1)"\n"
	"ldst %1, %2\n"
	"rli %sr, 80b; jnz %1, %sr\n"
	"li8 %1, '+'; st %1, %2\n"
	//"li8 %1, '\n'; st %1, %2\n"
	#endif

	// Assumption is that the interrupt
	// is always occurring only for:
	// ReadFaultIntr WriteFaultIntr ExecFaultIntr.

	"getfaultreason %1\n"
	"li %2, 2\n" // ExecFaultIntr.
	"li %3, 0b10001\n" // user::::executable:
	"seq %1, %2; rli %sr, 10f; jnz %1, %sr\n"
	"li %3, 0b10110; 10:\n" // user::readable:writable::

	"getfaultaddr %1\n"

	"li %2, 0xfff\n"
	"li %4, 3\n"
	"not %5, %2\n"

	"and %1, %5\n"
	"cpy %6, %1\n"
	"or %6, %3\n"
	// Enable caching if page is not at address 0.
	"cpy %7, %1\n"
	"slte %7, %2\n"
	"sll %7, %4\n"
	"or %6, %7\n"

	"settlb %6, %1\n"

	#ifdef DEBUG_KERNELMODE
	"80: li %2, "__xstr__(UARTADDR)"\n"
	"li %1, "__xstr__((HWDRVCHAR_CMDGETBUFFERUSAGE<<30) | 1)"\n"
	"ldst %1, %2\n"
	"rli %sr, 80b; jnz %1, %sr\n"
	"li8 %1, '-'; st %1, %2\n"
	//"li8 %1, '\n'; st %1, %2\n"
	#endif

	"rli %sr, 0b; j %sr; 1:\n"

	// Initialize %sp and %fp.
	"rli %sp, stack + "__xstr__(STACKSZ)"\n"
	"li8 %fp, 0\n"
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
	"j %rp\n"

	".size    _start, (. - _start)\n");

#include <hwdrvchar/hwdrvchar.h>
hwdrvchar hwdrvchar_dev = {.addr = (void *)UARTADDR};

int putchar (int c) {
	while (!hwdrvchar_write(&hwdrvchar_dev, &c, 1));
	return c;
}

#include <stdio.h>

#define puts_hex(I) ({ \
	inline unsigned char digit (unsigned char c) { \
		c = (c+((c>=10)?('a'-10):'0')); \
		return c; \
	} \
	unsigned i, j; \
	if (!(I)) \
		putchar('0'); \
	else for (i = 0, j = 0; i < (2*sizeof(I)); ++i) { \
		unsigned char c = (I>>(((8*sizeof(I))-4)-(i*4))); \
		if (j |= c) \
			putchar(digit(c&0xf)); \
	} \
	i; \
})

unsigned long mem8test (void *startaddr, void *endaddr);
#if __SIZEOF_POINTER__ >= 2
unsigned long mem16test (void *startaddr, void *endaddr);
#endif
#if __SIZEOF_POINTER__ >= 4
unsigned long mem32test (void *startaddr, void *endaddr);
#endif
#if __SIZEOF_POINTER__ >= 8
unsigned long mem64test (void *startaddr, void *endaddr);
#endif

#include <stdint.h>

typedef union {
	struct {
		unsigned long lo, hi;
	};
	uint64_t val;
} clkcyclecnt;

clkcyclecnt getclkcyclecnt (void) {
	inline unsigned long getclkcyclecnthi (void) {
		unsigned long hi;
		asm volatile ("getclkcyclecnth %0\n" : "=r"(hi));
		return hi;
	}
	clkcyclecnt ret;
	do {
		ret.hi = getclkcyclecnthi();
		asm volatile ("getclkcyclecnt %0\n"  : "=r"(ret.lo));
	} while (ret.hi != getclkcyclecnthi());
	return ret;
}

unsigned long progressmodulo;

#include <hwdrvdevtbl/hwdrvdevtbl.h>

unsigned long PATTERN1 = (unsigned long)0xaaaaaaaaaaaaaaaa;
unsigned long PATTERN2 = (unsigned long)0xc9c9c9c9c9c9c9c9;

void main (void) {
	hwdrvchar_init (&hwdrvchar_dev, UARTBAUD);
	hwdrvdevtbl hwdrvdevtbl_dev = {.e = (devtblentry *)0, .id = 1 /* RAM device */};
	hwdrvdevtbl_find (&hwdrvdevtbl_dev, 0);
	if (!hwdrvdevtbl_dev.mapsz) {
		puts("! memory not found\n");
		return; }
	void *memstartaddr = hwdrvdevtbl_dev.addr;
	void *memendaddr = memstartaddr + (hwdrvdevtbl_dev.mapsz * sizeof(unsigned long));
	progressmodulo = ((memendaddr - memstartaddr) / 0xff);
	test_begin:; clkcyclecnt startclkcyclecnt = getclkcyclecnt();
	puts("8bits test\n");
	if (!mem8test (memstartaddr, memendaddr))
		return;
	#if __SIZEOF_POINTER__ >= 2
	puts("16bits test\n");
	if (!mem16test (memstartaddr, memendaddr))
		return;
	#endif
	#if __SIZEOF_POINTER__ >= 4
	puts("32bits test\n");
	if (!mem32test (memstartaddr, memendaddr))
		return;
	#endif
	#if __SIZEOF_POINTER__ >= 8
	puts("64bits test\n");
	if (!mem64test (memstartaddr, memendaddr))
		return;
	#endif
	clkcyclecnt endclkcyclecnt = getclkcyclecnt();
	endclkcyclecnt.val -= startclkcyclecnt.val;
	puts("done ");
	puts_hex(endclkcyclecnt.hi);
	putchar (':');
	puts_hex(endclkcyclecnt.lo);
	putchar ('\n');
	PATTERN1 -= 1;
	PATTERN2 += 1;
	goto test_begin;
}

extern void *__executable_start, *_end;

#define BITROL(VALUE, COUNT) \
	(((VALUE<<(COUNT&((8*sizeof(VALUE))-1)))|(VALUE>>((8*sizeof(VALUE))-(COUNT&((8*sizeof(VALUE))-1)))))&((typeof(VALUE))-1))

#define RESETLINE "\r\033[K"

unsigned long mem8test (void *startaddr, void *endaddr) {

	uint8_t data = (uint8_t)PATTERN1;
	for (volatile uint8_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		data = (BITROL (data, 1) + *(uint8_t *)&ptr);
		*ptr = data;
		uint8_t ptrval = *ptr;
		if (ptrval != data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data); putchar('\n');
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"wrote pattern\n");

	// Invert pattern written above.
	for (volatile uint8_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		// This test the instruction ldst.
		data = (uint8_t)PATTERN2;
		__asm__ __volatile__ ("ldst8 %0, %1" : "+r"(data) : "r" (ptr));
		uint8_t ptrval = *ptr;
		if (ptrval != (uint8_t)PATTERN2) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data);
			return 0;
		}
		data = ~data;
		*ptr = data;
		ptrval = *ptr;
		if (ptrval != data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data);
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"wrote inverted pattern\n");

	// Retrieve inverted pattern written above.
	data = (uint8_t)PATTERN1;
	for (volatile uint8_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		data = (BITROL (data, 1) + *(uint8_t *)&ptr);
		uint8_t ptrval = *ptr;
		if (ptrval != (uint8_t)~data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex((uint8_t)~data); putchar('\n');
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"verified pattern\n");

	data = (uint8_t)PATTERN2;

	for (volatile uint8_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		data += 1;
		*ptr = data;
		uint8_t ptrval = *ptr;
		if (ptrval != data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data); putchar('\n');
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"wrote retention pattern\n");

	data = (uint8_t)PATTERN2;

	// Retrieve pattern written above.
	for (volatile uint8_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		data += 1;
		uint8_t ptrval = *ptr;
		if (ptrval != data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data); putchar('\n');
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"verified pattern\n");

	return 1;
}

#if __SIZEOF_POINTER__ >= 2
unsigned long mem16test (void *startaddr, void *endaddr) {

	uint16_t data = (uint16_t)PATTERN1;
	for (volatile uint16_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		data = (BITROL (data, 1) + *(uint16_t *)&ptr);
		*ptr = data;
		uint16_t ptrval = *ptr;
		if (ptrval != data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data); putchar('\n');
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"wrote pattern\n");

	// Invert pattern written above.
	for (volatile uint16_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		// This test the instruction ldst.
		data = (uint16_t)PATTERN2;
		__asm__ __volatile__ ("ldst16 %0, %1" : "+r"(data) : "r" (ptr));
		uint16_t ptrval = *ptr;
		if (ptrval != (uint16_t)PATTERN2) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data); putchar('\n');
			return 0;
		}
		data = ~data;
		*ptr = data;
		ptrval = *ptr;
		if (ptrval != data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data); putchar('\n');
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"wrote inverted pattern\n");

	// Retrieve inverted pattern written above.
	data = (uint16_t)PATTERN1;
	for (volatile uint16_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		data = (BITROL (data, 1) + *(uint16_t *)&ptr);
		uint16_t ptrval = *ptr;
		if (ptrval != (uint16_t)~data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex((uint16_t)~data); putchar('\n');
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"verified pattern\n");

	data = (uint16_t)PATTERN2;

	for (volatile uint16_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		data += 1;
		*ptr = data;
		uint16_t ptrval = *ptr;
		if (ptrval != data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data); putchar('\n');
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"wrote retention pattern\n");

	data = (uint16_t)PATTERN2;

	// Retrieve pattern written above.
	for (volatile uint16_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		data += 1;
		uint16_t ptrval = *ptr;
		if (ptrval != data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data); putchar('\n');
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"verified pattern\n");

	return 1;
}
#endif

#if __SIZEOF_POINTER__ >= 4
unsigned long mem32test (void *startaddr, void *endaddr) {

	uint32_t data = (uint32_t)PATTERN1;
	for (volatile uint32_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		data = (BITROL (data, 1) + *(uint32_t *)&ptr);
		*ptr = data;
		uint32_t ptrval = *ptr;
		if (ptrval != data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data); putchar('\n');
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"wrote pattern\n");

	// Invert pattern written above.
	for (volatile uint32_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		// This test the instruction ldst.
		data = (uint32_t)PATTERN2;
		__asm__ __volatile__ ("ldst32 %0, %1" : "+r"(data) : "r" (ptr));
		uint32_t ptrval = *ptr;
		if (ptrval != (uint32_t)PATTERN2) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data); putchar('\n');
			return 0;
		}
		data = ~data;
		*ptr = data;
		ptrval = *ptr;
		if (ptrval != data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data); putchar('\n');
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"wrote inverted pattern\n");

	// Retrieve inverted pattern written above.
	data = (uint32_t)PATTERN1;
	for (volatile uint32_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		data = (BITROL (data, 1) + *(uint32_t *)&ptr);
		uint32_t ptrval = *ptr;
		if (ptrval != (uint32_t)~data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex((uint32_t)~data); putchar('\n');
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"verified pattern\n");

	data = (uint32_t)PATTERN2;

	for (volatile uint32_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		data += 1;
		*ptr = data;
		uint32_t ptrval = *ptr;
		if (ptrval != data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data); putchar('\n');
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"wrote retention pattern\n");

	data = (uint32_t)PATTERN2;

	// Retrieve pattern written above.
	for (volatile uint32_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		data += 1;
		uint32_t ptrval = *ptr;
		if (ptrval != data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data); putchar('\n');
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"verified pattern\n");

	return 1;
}
#endif

#if __SIZEOF_POINTER__ >= 8
unsigned long mem64test (void *startaddr, void *endaddr) {

	uint64_t data = (uint64_t)PATTERN1;
	for (volatile uint64_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		data = (BITROL (data, 1) + *(uint64_t *)&ptr);
		*ptr = data;
		uint64_t ptrval = *ptr;
		if (ptrval != data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data); putchar('\n');
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"wrote pattern\n");

	// Invert pattern written above.
	for (volatile uint64_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		// This test the instruction ldst.
		data = (uint64_t)PATTERN2;
		__asm__ __volatile__ ("ldst64 %0, %1" : "+r"(data) : "r" (ptr));
		uint64_t ptrval = *ptr;
		if (ptrval != (uint64_t)PATTERN2) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data); putchar('\n');
			return 0;
		}
		data = ~data;
		*ptr = data;
		ptrval = *ptr;
		if (ptrval != data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data); putchar('\n');
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"wrote inverted pattern\n");

	// Retrieve inverted pattern written above.
	data = (uint64_t)PATTERN1;
	for (volatile uint64_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		data = (BITROL (data, 1) + *(uint64_t *)&ptr);
		uint64_t ptrval = *ptr;
		if (ptrval != (uint64_t)~data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex((uint64_t)~data); putchar('\n');
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"verified pattern\n");

	data = (uint64_t)PATTERN2;

	for (volatile uint64_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		data += 1;
		*ptr = data;
		uint64_t ptrval = *ptr;
		if (ptrval != data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data); putchar('\n');
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"wrote retention pattern\n");

	data = (uint64_t)PATTERN2;

	// Retrieve pattern written above.
	for (volatile uint64_t *ptr = startaddr; ptr < (typeof(ptr))endaddr; ++ptr) {
		if (ptr >= (typeof(ptr))&__executable_start && ptr < (typeof(ptr))&_end)
			ptr = (typeof(ptr))&_end;
		data += 1;
		uint64_t ptrval = *ptr;
		if (ptrval != data) {
			puts(RESETLINE"ptr    == 0x"); puts_hex((unsigned)ptr);
			puts("\nptrval == 0x"); puts_hex(ptrval);
			puts("\nexpect == 0x"); puts_hex(data); putchar('\n');
			return 0;
		}
		unsigned long remaining = (unsigned long)endaddr - (unsigned long)ptr;
		if (!(remaining%progressmodulo)) {
			puts(RESETLINE); puts_hex(remaining/progressmodulo);
		}
	}

	puts(RESETLINE"verified pattern\n");

	return 1;
}
#endif
