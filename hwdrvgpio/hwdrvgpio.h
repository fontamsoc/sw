// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef HWDRVGPIO_H
#define HWDRVGPIO_H

// Structure representing a GPIO device.
// The field addr must be valid.
typedef struct {
	// Device address.
	void* addr;
	// Count of IOs.
	unsigned long iocnt;
	// Clock frequency in Hz used by the device.
	unsigned long clkfreq;
} hwdrvgpio;

// Commands.
#define HWDRVGPIO_CMDCONFIGUREIO ((unsigned long)0)
#define HWDRVGPIO_CMDSETDEBOUNCE ((unsigned long)1)

// Configure IOs where "arg" is a bitmap where each bit
// 0/1 configures corresponding IO as an input/output.
// dev->iocnt gets set.
static inline void hwdrvgpio_configureio (hwdrvgpio *dev, unsigned long arg) {
	dev->iocnt = ((HWDRVGPIO_CMDCONFIGUREIO<<((sizeof(unsigned long)*8)-1))|(arg&(((unsigned long)1<<((sizeof(unsigned long)*8)-1))-1)));
	__asm__ __volatile__ (
		"ldst %0, %1"
		: "+r" (dev->iocnt)
		: "r" (dev->addr)
		: "memory");
}

// Configure clockcycle count ("arg") used to debounce inputs.
// dev->clkfreq gets set.
static inline void hwdrvgpio_setdebounce (hwdrvgpio *dev, unsigned long arg) {
	dev->clkfreq = ((HWDRVGPIO_CMDSETDEBOUNCE<<((sizeof(unsigned long)*8)-1))|(arg&(((unsigned long)1<<((sizeof(unsigned long)*8)-1))-1)));
	__asm__ __volatile__ (
		"ldst %0, %1"
		: "+r" (dev->clkfreq)
		: "r" (dev->addr)
		: "memory");
}

#endif /* HWDRVGPIO_H */
