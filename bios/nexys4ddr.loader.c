// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

// nexys4ddr.loader must fit in the MBR first 446 bytes.
// It implements only what is necessary to initialize
// the RAM and load the next stage which is the loader.

#include <stdint.h>

// Used to stringify.
#define __xstr__(s) __str__(s)
#define __str__(s) #s

static unsigned long stack[STACKSZ] __attribute__((used));

// Substitute for crt0.S since this is built using -nostdlib.
__asm__ (
	".section .text._start\n"
	".global  _start\n"
	".type    _start, @function\n"
	".p2align 1\n"
	"_start:\n"

	// Adjust %ksl to enable caching throughout the memory
	// region where the nexys4ddr.loader will be running.
	"li %sr, 512\n"
	"setksl %sr\n"
	// Initialize %sp and %fp.
	"rli16 %sp, stack + "__xstr__(STACKSZ)"\n" // ### Manually encoding rli16 since linker-relaxation is not yet implemented.
	//"li8 %fp, 0\n" // ### Disabled, as it is unnecessary.
	// Call main().
	"rli16 %sr, main\n" // ### Manually encoding rli16 since linker-relaxation is not yet implemented.
	"jl %rp, %sr\n"
	"li8 %rp, 0\n"
	#ifdef NESYS4DDR_LOADER_PART0
	"li %1, "__xstr__(NEXTLOADER)"\n"
	#else
	"li %1, "__xstr__((NEXTLOADER+1))"\n"
	#endif
	"rli16 %sr, blkdev_read\n" // ### Manually encoding rli16 since linker-relaxation is not yet implemented.
	"j %sr\n"
	// Returning from blkdev_read() will continue execution at 0x0.

	".size    _start, (. - _start)\n");

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

static void cdelay (unsigned long i) {
	asm volatile ( // At least 4 clock-cycles each loop.
		"rli8 %%sr, 0f; rli8 %1, 1f; 0:\n"
		"jz %0, %1; inc8 %0, -1; j %%sr; 1:\n"
		:: "r"(i), "r"((unsigned long){0}));
}

#define DFII_CONTROL_SEL        0x01
#define DFII_CONTROL_CKE        0x02
#define DFII_CONTROL_ODT        0x04
#define DFII_CONTROL_RESET_N    0x08

#define DFII_COMMAND_CS         0x01
#define DFII_COMMAND_WE         0x02
#define DFII_COMMAND_CAS        0x04
#define DFII_COMMAND_RAS        0x08
#define DFII_COMMAND_WRDATA     0x10
#define DFII_COMMAND_RDDATA     0x20

/* CSR subregisters (a.k.a. "simple CSRs") are embedded inside uint32_t
 * aligned locations: */
#define MMPTR(a) (*((volatile uint32_t *)(a)))

static unsigned long csr_read_simple (unsigned long a) {
	return MMPTR(a);
}
static void csr_write_simple (unsigned long v, unsigned long a) {
	MMPTR(a) = v;
}

static void sdram_dfii_pi0_address_write (unsigned long v) {
	csr_write_simple(v >> 8, CSR_BASE + 0x80cL);
	csr_write_simple(v, CSR_BASE + 0x810L);
}

static void sdram_dfii_pi0_baddress_write (unsigned long v) {
	csr_write_simple(v, CSR_BASE + 0x814L);
}

static unsigned long sdram_dfii_control_read (void) {
	return csr_read_simple(CSR_BASE + 0x800L);
}

static void sdram_dfii_control_write (unsigned long v) {
	csr_write_simple(v, CSR_BASE + 0x800L);
}

static void sdram_dfii_pi0_command_write (unsigned long v) {
	csr_write_simple(v, CSR_BASE + 0x804L);
}

static void sdram_dfii_pi0_command_issue_write (unsigned long v) {
	csr_write_simple(v, CSR_BASE + 0x808L);
}

static void command_p0 (unsigned long cmd) {
	sdram_dfii_pi0_command_write(cmd);
	sdram_dfii_pi0_command_issue_write(1);
}

#ifdef NESYS4DDR_LOADER_PART1

static void sdram_dfii_pi1_address_write (unsigned long v) {
	csr_write_simple(v >> 8, CSR_BASE + 0x840L);
	csr_write_simple(v, CSR_BASE + 0x844L);
}

static void sdram_dfii_pi1_baddress_write (unsigned long v) {
	csr_write_simple(v, CSR_BASE + 0x848L);
}

static unsigned long ddrphy_rdphase_read (void) {
	return csr_read_simple(CSR_BASE + 0x2cL);
}

static unsigned long ddrphy_wrphase_read (void) {
	return csr_read_simple(CSR_BASE + 0x30L);
}

static void sdram_dfii_pix_address_write (unsigned long phase, unsigned long value) {
	switch (phase) {
		case 1: sdram_dfii_pi1_address_write(value); break;
		default: sdram_dfii_pi0_address_write(value);
	}
}

static void sdram_dfii_pird_address_write (unsigned long value) {
	unsigned long rdphase = ddrphy_rdphase_read();
	sdram_dfii_pix_address_write(rdphase, value);
}

static void sdram_dfii_piwr_address_write (unsigned long value) {
	unsigned long wrphase = ddrphy_wrphase_read();
	sdram_dfii_pix_address_write(wrphase, value);
}

static void sdram_dfii_pix_baddress_write (unsigned long phase, unsigned long value) {
	switch (phase) {
		case 1: sdram_dfii_pi1_baddress_write(value); break;
		default: sdram_dfii_pi0_baddress_write(value);
	}
}

static void sdram_dfii_pird_baddress_write (unsigned long value) {
	unsigned long rdphase = ddrphy_rdphase_read();
	sdram_dfii_pix_baddress_write(rdphase, value);
}

static void sdram_dfii_piwr_baddress_write (unsigned long value) {
	unsigned long wrphase = ddrphy_wrphase_read();
	sdram_dfii_pix_baddress_write(wrphase, value);
}

static void sdram_dfii_pi1_command_write (unsigned long v) {
	csr_write_simple(v, CSR_BASE + 0x838L);
}

static void sdram_dfii_pi1_command_issue_write (unsigned long v) {
	csr_write_simple(v, CSR_BASE + 0x83cL);
}

static void command_p1 (unsigned long cmd) {
    sdram_dfii_pi1_command_write(cmd);
    sdram_dfii_pi1_command_issue_write(1);
}

static void command_px (unsigned long phase, unsigned long value) {
	switch (phase) {
		case 1: command_p1(value); break;
		default: command_p0(value);
	}
}

static void command_prd (unsigned long value) {
	unsigned long rdphase = ddrphy_rdphase_read();
	command_px(rdphase, value);
}

static void command_pwr (unsigned long value) {
	unsigned long wrphase = ddrphy_wrphase_read();
	command_px(wrphase, value);
}

#endif

#define SDRAM_PHY_RDPHASE 1
#define SDRAM_PHY_WRPHASE 0

static void ddrphy_rdphase_write (unsigned long v) {
	csr_write_simple(v, CSR_BASE + 0x2cL);
}

static void ddrphy_wrphase_write (unsigned long v) {
	csr_write_simple(v, CSR_BASE + 0x30L);
}

static void ddrphy_rst_write (unsigned long v) {
	csr_write_simple(v, CSR_BASE + 0x0L);
}

static void ddrctrl_init_done_write (unsigned long v) {
	csr_write_simple(v, CSR_BASE + 0x1000L);
}

static void ddrctrl_init_error_write (unsigned long v) {
	csr_write_simple(v, CSR_BASE + 0x1004L);
}

#define DFII_CONTROL_SOFTWARE (DFII_CONTROL_CKE|DFII_CONTROL_ODT|DFII_CONTROL_RESET_N)
#define DFII_CONTROL_HARDWARE (DFII_CONTROL_SEL)

static void sdram_software_control_on (void) {
	unsigned long previous;
	previous = sdram_dfii_control_read();
	/* Switch DFII to software control */
	if (previous != DFII_CONTROL_SOFTWARE) {
		sdram_dfii_control_write(DFII_CONTROL_SOFTWARE);
	}
}

#ifdef NESYS4DDR_LOADER_PART1

void sdram_software_control_off (void) {
	unsigned long previous;
	previous = sdram_dfii_control_read();
	/* Switch DFII to hardware control */
	if (previous != DFII_CONTROL_HARDWARE) {
		sdram_dfii_control_write(DFII_CONTROL_HARDWARE);
	}
}

static void ddrphy_dly_sel_write (unsigned long v) {
	csr_write_simple(v, CSR_BASE + 0x10L);
}

static void ddrphy_rdly_dq_rst_write (unsigned long v) {
	csr_write_simple(v, CSR_BASE + 0x14L);
}

static void sdram_read_leveling_rst_delay (unsigned long module) {
	/* Select module */
	ddrphy_dly_sel_write(1 << module);
	/* Reset delay */
	ddrphy_rdly_dq_rst_write(1);
	/* Un-select module */
	ddrphy_dly_sel_write(0);
}

static void ddrphy_rdly_dq_bitslip_rst_write (unsigned long v) {
	csr_write_simple(v, CSR_BASE + 0x1cL);
}

static void sdram_read_leveling_rst_bitslip (unsigned long m) {
	/* Select module */
	ddrphy_dly_sel_write(1 << m);
	/* Reset delay */
	ddrphy_rdly_dq_bitslip_rst_write(1);
	/* Un-select module */
	ddrphy_dly_sel_write(0);
}

static void ddrphy_wdly_dq_bitslip_rst_write (unsigned long v) {
	csr_write_simple(v, CSR_BASE + 0x24L);
}

static void ddrphy_wdly_dq_bitslip_write (unsigned long v) {
	csr_write_simple(v, CSR_BASE + 0x28L);
}

static void sdram_activate_test_row (void) {
	sdram_dfii_pi0_address_write(0);
	sdram_dfii_pi0_baddress_write(0);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CS);
	cdelay(15);
}

static void sdram_precharge_test_row (void) {
	sdram_dfii_pi0_address_write(0);
	sdram_dfii_pi0_baddress_write(0);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_WE|DFII_COMMAND_CS);
	cdelay(15);
}

#define CONFIG_CSR_DATA_WIDTH 8
#define CSR_SDRAM_DFII_PI0_WRDATA_SIZE 4
#define DFII_PIX_DATA_SIZE CSR_SDRAM_DFII_PI0_WRDATA_SIZE
#define DFII_PIX_DATA_BYTES DFII_PIX_DATA_SIZE*CONFIG_CSR_DATA_WIDTH/8
#define SDRAM_PHY_MODULES DFII_PIX_DATA_BYTES/2
#define SDRAM_PHY_PHASES 2
#include "lfsr.h"
#define CSR_SDRAM_DFII_PI0_WRDATA_ADDR (CSR_BASE + 0x818L)
#define CSR_SDRAM_DFII_PI1_WRDATA_ADDR (CSR_BASE + 0x84cL)
const unsigned long sdram_dfii_pix_wrdata_addr[SDRAM_PHY_PHASES] = {
	CSR_SDRAM_DFII_PI0_WRDATA_ADDR,
	CSR_SDRAM_DFII_PI1_WRDATA_ADDR
};
#define CSR_SDRAM_DFII_PI0_RDDATA_ADDR (CSR_BASE + 0x828L)
#define CSR_SDRAM_DFII_PI1_RDDATA_ADDR (CSR_BASE + 0x85cL)
const unsigned long sdram_dfii_pix_rddata_addr[SDRAM_PHY_PHASES] = {
	CSR_SDRAM_DFII_PI0_RDDATA_ADDR,
	CSR_SDRAM_DFII_PI1_RDDATA_ADDR
};
static unsigned long sdram_write_read_check_test_pattern (unsigned long module, unsigned long seed) {
	unsigned long prs[SDRAM_PHY_PHASES];
	/* Generate pseudo-random sequence */
	unsigned long prv = seed;
	for (unsigned long p=0;p<SDRAM_PHY_PHASES;p++) {
		prv = lfsr(32, prv);
		prs[p] = prv;
	}
	/* Activate */
	sdram_activate_test_row();
	/* Write pseudo-random sequence */
	for (unsigned long p=0;p<SDRAM_PHY_PHASES;p++)
		csr_write_simple(prs[p], sdram_dfii_pix_wrdata_addr[p]);
	sdram_dfii_piwr_address_write(0);
	sdram_dfii_piwr_baddress_write(0);
	command_pwr(DFII_COMMAND_CAS|DFII_COMMAND_WE|DFII_COMMAND_CS|DFII_COMMAND_WRDATA);
	cdelay(15);
	/* Read/Check pseudo-random sequence */
	sdram_dfii_pird_address_write(0);
	sdram_dfii_pird_baddress_write(0);
	command_prd(DFII_COMMAND_CAS|DFII_COMMAND_CS|DFII_COMMAND_RDDATA);
	cdelay(15);
	/* Precharge */
	sdram_precharge_test_row();
	for (unsigned long p=0;p<SDRAM_PHY_PHASES;p++) {
		/* Read back test pattern and Verify bytes matching current 'module' */
		if (prs[p] != csr_read_simple(sdram_dfii_pix_rddata_addr[p]))
			return 0;
	}
	return 1;
}

static void ddrphy_rdly_dq_inc_write (unsigned long v) {
	csr_write_simple(v, CSR_BASE + 0x18L);
}

static void sdram_read_leveling_inc_delay (unsigned long module) {
	/* Select module */
	ddrphy_dly_sel_write(1 << module);
	/* Increment delay */
	ddrphy_rdly_dq_inc_write(1);
	/* Un-select module */
	ddrphy_dly_sel_write(0);
}

#define SDRAM_PHY_DELAYS 32
static long sdram_read_leveling_scan_module (unsigned long module) {
	/* Check test pattern for each delay value */
	unsigned long score = 0;
	sdram_read_leveling_rst_delay(module);
	for (unsigned long i=0;i<SDRAM_PHY_DELAYS;i++) {
		unsigned long working;
		working  = sdram_write_read_check_test_pattern(module, 42);
		working &= sdram_write_read_check_test_pattern(module, 84);
		score += working;
		sdram_read_leveling_inc_delay(module);
	}
	return score;
}

static void ddrphy_rdly_dq_bitslip_write (unsigned long v) {
	csr_write_simple(v, CSR_BASE + 0x20L);
}

static void sdram_read_leveling_inc_bitslip (unsigned long m) {
	/* Select module */
	ddrphy_dly_sel_write(1 << m);
	/* Increment delay */
	ddrphy_rdly_dq_bitslip_write(1);
	/* Un-select module */
	ddrphy_dly_sel_write(0);
}

static void sdram_read_leveling_module (unsigned long module) {
	long i;
	long working;
	long delay, delay_min, delay_max;
	/* Find smallest working delay */
	delay = 0;
	sdram_read_leveling_rst_delay(module);
	while(1) {
		working  = sdram_write_read_check_test_pattern(module, 42);
		working &= sdram_write_read_check_test_pattern(module, 84);
		if(working)
			break;
		delay++;
		if(delay >= SDRAM_PHY_DELAYS)
			break;
		sdram_read_leveling_inc_delay(module);
	}
	delay_min = delay;
	/* Get a bit further into the working zone */
	delay++;
	sdram_read_leveling_inc_delay(module);
	/* Find largest working delay */
	while (1) {
		working  = sdram_write_read_check_test_pattern(module, 42);
		working &= sdram_write_read_check_test_pattern(module, 84);
		if(!working)
			break;
		delay++;
		if(delay >= SDRAM_PHY_DELAYS)
			break;
		sdram_read_leveling_inc_delay(module);
	}
	delay_max = delay;
	/* Set delay to the middle */
	sdram_read_leveling_rst_delay(module);
	for(i=0;i<(delay_min+delay_max)/2;i++) {
		sdram_read_leveling_inc_delay(module);
		cdelay(100);
	}
}

#endif /* NESYS4DDR_LOADER_PART1 */

void main (void) {
	#ifdef NESYS4DDR_LOADER_PART0
	ddrphy_rdphase_write(SDRAM_PHY_RDPHASE);
	ddrphy_wrphase_write(SDRAM_PHY_WRPHASE);
	sdram_software_control_on();
	ddrphy_rst_write(1);
	cdelay(1000);
	ddrphy_rst_write(0);
	cdelay(1000);
	ddrctrl_init_done_write(0);
	ddrctrl_init_error_write(0);
	/* Bring CKE high */
	sdram_dfii_pi0_address_write(0x0);
	sdram_dfii_pi0_baddress_write(0);
	sdram_dfii_control_write(DFII_CONTROL_CKE|DFII_CONTROL_ODT|DFII_CONTROL_RESET_N);
	cdelay(20000);
	/* Precharge All */
	sdram_dfii_pi0_address_write(0x400);
	sdram_dfii_pi0_baddress_write(0);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_WE|DFII_COMMAND_CS);
	/* Load Extended Mode Register 3 */
	sdram_dfii_pi0_address_write(0x0);
	sdram_dfii_pi0_baddress_write(3);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CAS|DFII_COMMAND_WE|DFII_COMMAND_CS);
	/* Load Extended Mode Register 2 */
	sdram_dfii_pi0_address_write(0x0);
	sdram_dfii_pi0_baddress_write(2);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CAS|DFII_COMMAND_WE|DFII_COMMAND_CS);
	/* Load Extended Mode Register */
	sdram_dfii_pi0_address_write(0x0);
	sdram_dfii_pi0_baddress_write(1);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CAS|DFII_COMMAND_WE|DFII_COMMAND_CS);
	/* Load Mode Register / Reset DLL, CL=3, BL=4 */
	sdram_dfii_pi0_address_write(0x532);
	sdram_dfii_pi0_baddress_write(0);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CAS|DFII_COMMAND_WE|DFII_COMMAND_CS);
	cdelay(200);
	/* Precharge All */
	sdram_dfii_pi0_address_write(0x400);
	sdram_dfii_pi0_baddress_write(0);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_WE|DFII_COMMAND_CS);
	#else /* NESYS4DDR_LOADER_PART0 */
	/* Auto Refresh */
	sdram_dfii_pi0_address_write(0x0);
	sdram_dfii_pi0_baddress_write(0);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CAS|DFII_COMMAND_CS);
	cdelay(4);
	/* Auto Refresh */
	sdram_dfii_pi0_address_write(0x0);
	sdram_dfii_pi0_baddress_write(0);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CAS|DFII_COMMAND_CS);
	cdelay(4);
	/* Load Mode Register / CL=3, BL=4 */
	sdram_dfii_pi0_address_write(0x432);
	sdram_dfii_pi0_baddress_write(0);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CAS|DFII_COMMAND_WE|DFII_COMMAND_CS);
	cdelay(200);
	/* Load Extended Mode Register / OCD Default */
	sdram_dfii_pi0_address_write(0x380);
	sdram_dfii_pi0_baddress_write(1);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CAS|DFII_COMMAND_WE|DFII_COMMAND_CS);
	/* Load Extended Mode Register / OCD Exit */
	sdram_dfii_pi0_address_write(0x0);
	sdram_dfii_pi0_baddress_write(1);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CAS|DFII_COMMAND_WE|DFII_COMMAND_CS);
	/* void sdram_leveling (void) */ {
		sdram_software_control_on();
		for(unsigned long module=0; module<SDRAM_PHY_MODULES; module++) {
			sdram_read_leveling_rst_delay(module);
			sdram_read_leveling_rst_bitslip(module);
		}
		#define SDRAM_PHY_BITSLIPS 8
		/* void sdram_write_latency_calibration (void) */ {
			long bitslip;
			long score;
			long best_score;
			long best_bitslip;
			for (unsigned long module=0; module<SDRAM_PHY_MODULES; module++) {
				/* Scan possible write windows */
				best_score = 0;
				best_bitslip = -1;
				for (bitslip=0; bitslip<SDRAM_PHY_BITSLIPS; bitslip+=2) { /* +2 for tCK steps */
					score = 0;
					/* Select module */
					ddrphy_dly_sel_write(1 << module);
					/* Reset bitslip */
					ddrphy_wdly_dq_bitslip_rst_write(1);
					for (unsigned long i=0; i<bitslip; i++) {
						ddrphy_wdly_dq_bitslip_write(1);
					}
					/* Un-select module */
					ddrphy_dly_sel_write(0);
					score = 0;
					sdram_read_leveling_rst_bitslip(module);
					for (unsigned long i=0; i<SDRAM_PHY_BITSLIPS; i++) {
						/* Compute score */
						score += sdram_read_leveling_scan_module(module);
						/* Increment bitslip */
						sdram_read_leveling_inc_bitslip(module);
					}
					if (score > best_score) {
						best_bitslip = bitslip;
						best_score = score;
					}
				}

				bitslip = best_bitslip;

				/* Select best write window */
				ddrphy_dly_sel_write(1 << module);

				/* Reset bitslip */
				ddrphy_wdly_dq_bitslip_rst_write(1);
				for (unsigned long i=0; i<bitslip; i++) {
					ddrphy_wdly_dq_bitslip_write(1);
				}
				/* Un-select module */
				ddrphy_dly_sel_write(0);
			}
		}
		/* void sdram_read_leveling (void) */ {
			long bitslip;
			long score;
			long best_score;
			long best_bitslip;
			for (unsigned long module=0; module<SDRAM_PHY_MODULES; module++) {
				/* Scan possible read windows */
				best_score = 0;
				best_bitslip = 0;
				for (bitslip=0; bitslip<SDRAM_PHY_BITSLIPS; bitslip++) {
					/* Compute score */
					score = sdram_read_leveling_scan_module(module);
					sdram_read_leveling_module(module);
					if (score > best_score) {
						best_bitslip = bitslip;
						best_score = score;
					}
					/* Exit */
					if (bitslip == SDRAM_PHY_BITSLIPS-1)
						break;
					/* Increment bitslip */
					sdram_read_leveling_inc_bitslip(module);
				}

				/* Select best read window */
				sdram_read_leveling_rst_bitslip(module);
				for (bitslip=0; bitslip<best_bitslip; bitslip++)
					sdram_read_leveling_inc_bitslip(module);

				/* Re-do leveling on best read window*/
				sdram_read_leveling_module(module);
			}
		}
		sdram_software_control_off();
	}
	//sdram_software_control_off(); // Already done by sdram_leveling().sdram_read_leveling()
	ddrctrl_init_done_write(1);
	#endif /* NESYS4DDR_LOADER_PART0 */
}
