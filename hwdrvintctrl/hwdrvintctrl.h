// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef HWDRVINTCTRL_H
#define HWDRVINTCTRL_H

// PIRWOP is the only memory operation handled with following
// expected from pi1_data_i |arg: (ARCHBITSZ-2) bits|cmd: 2 bit|
// where the field "cmd" values are CMDACKINT(2'b00), CMDINTDST(2'b01) or CMDENAINT(2'b10).
// 	CMDACKINT: Acknowledges an interrupt source; field "arg" is expected
// 	to have following format |idx: (ARCHBITSZ-3) bits|en: 1 bit|
// 	where "idx" is the interrupt destination index, "en" enable/disable
// 	further interrupt delivery to the interrupt destination "idx".
// 	pi1_data_o get set to the interrupt source index, or -2 if
// 	there are no pending interrupt for the destination "idx",
// 	or -1 (for an interrupt triggered by CMDINTDST).
// 	CMDINTDST: Triggers an interrupt targeting a specific destination;
// 	the field "arg" from pi1_data_i is the index of the interrupt destination
// 	to target, while pi1_data_o get set to the interrupt destination index
// 	if valid, -2 if not ready due to an interrupt pending ack, or -1 if invalid.
// 	CMDENAINT: Enable/Disable an interrupt source; field "arg" is expected
// 	to have following format |idx: (ARCHBITSZ-3) bits|en: 1 bit|
// 	where "idx" is the interrupt source index, "en" enable/disable
// 	interrupts from the interrupt source "idx".
// 	pi1_data_o get set to the interrupt source index, or -1 if invalid.
// An interrupt must be acknowledged as soon as possible so
// that the intctrl can dispatch another interrupt request.

#define INTCTRLADDR ((void *)0x0ff0 /* By convention, the interrupt controller is located at 0x0ff0 */)

static inline unsigned long hwdrvintctrl_ack (unsigned long idx, unsigned long en) {
	unsigned long intsrc = ((idx<<3) | ((en&1)<<2) | 0b00);
	__asm__ __volatile__ (
		"ldst %0, %1"
		: "+r" (intsrc)
		: "r" (INTCTRLADDR)
		: "memory");
	return intsrc;
}

static inline unsigned long hwdrvintctrl_int (unsigned long idx) {
	unsigned long intdst = ((idx<<2) | 0b01);
	__asm__ __volatile__ (
		"ldst %0, %1"
		: "+r" (intdst)
		: "r" (INTCTRLADDR)
		: "memory");
	return intdst;
}

static inline unsigned long hwdrvintctrl_ena (unsigned long idx, unsigned long en) {
	unsigned long intsrc = ((idx<<3) | ((en&1)<<2) | 0b10);
	__asm__ __volatile__ (
		"ldst %0, %1"
		: "+r" (intsrc)
		: "r" (INTCTRLADDR)
		: "memory");
	return intsrc;
}

#endif /* HWDRVINTCTRL_H */
