// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef HWDRVINTCTRL_H
#define HWDRVINTCTRL_H

// Commands are sent to the controller writing to it with the
// following expected format | arg: (ARCHBITSZ-2) bits | cmd: 2 bit |
// where the field "cmd" values are CMDDEVRDY(2'b00), CMDACKINT(2'b01),
// CMDINTDST(2'b10) and CMDENAINT(2'b11). The result of a previously
// sent command is retrieved from the controller reading from it and
// has the following format | resp: (ARCHBITSZ-2) bits | cmd: 2 bit |,
// where the fields "cmd" and "resp" are the command and its result.
// Two memory operations, a write followed by a read are needed to send
// a command to the controller and retrieve its result.
// The controller has accepted a command only if "cmd" in its result
// is CMDDEVRDY, otherwise sending the command CMDDEVRDY is needed.
//
// The description of commands is as follow:
// 	CMDDEVRDY: Make the controller accept a new command.
// 	"resp" in the result get set to 0.
// 	CMDACKINT: Acknowledges an interrupt source; field "arg" is expected
// 	to have following format | idx: (ARCHBITSZ-3) bits | en: 1 bit |
// 	where "idx" is the interrupt destination index, "en" enable/disable
// 	further interrupt delivery to the interrupt destination "idx".
// 	"resp" in the result get set to the interrupt source index, or -2
// 	if there are no pending interrupt for the destination "idx", or -1
// 	for an interrupt triggered by CMDINTDST.
// 	CMDINTDST: Triggers an interrupt targeting a specific destination;
// 	the field "arg" is the index of the interrupt destination to target,
// 	while "resp" in the result get set to the interrupt destination index
// 	if valid, -2 if not ready due to an interrupt pending ack, or -1 if invalid.
// 	CMDENAINT: Enable/Disable an interrupt source; field "arg" is expected
// 	to have following format | idx: (ARCHBITSZ-3) bits | en: 1 bit|
// 	where "idx" is the interrupt source index, "en" enable/disable
// 	interrupts from the interrupt source "idx".
// 	"resp" in the result get set to the interrupt source index, or -1
// 	if invalid.
//
// To be multi core proof, an atomic read-write must be used to send
// a command to the controller until CMDDEVRDY is returned, then another
// atomic read-write sending CMDDEVRDY must be used to retrieve the
// result while making the controller ready for the next command.
//
// An interrupt must be acknowledged as soon as possible so
// that the intctrl can dispatch another interrupt request.

#define INTCTRLADDR ((void *)0x0fe8 /* By convention, the interrupt controller is located at 0x0fe8 */)

static inline unsigned long hwdrvintctrl_ack (unsigned long idx, unsigned long en) {
	unsigned long intsrc;
	do {
		intsrc = ((idx<<3) | ((en&1)<<2) | 0b01);
		__asm__ __volatile__ (
			"ldst %0, %1"
			: "+r" (intsrc)
			: "r" (INTCTRLADDR)
			: "memory");
	} while (intsrc & 0b11);
	intsrc = 0;
	__asm__ __volatile__ (
		"ldst %0, %1"
		: "+r" (intsrc)
		: "r" (INTCTRLADDR)
		: "memory");
	return ((signed long)intsrc >> 2);
}

static inline unsigned long hwdrvintctrl_int (unsigned long idx) {
	unsigned long intdst;
	do {
		intdst = ((idx<<2) | 0b10);
		__asm__ __volatile__ (
			"ldst %0, %1"
			: "+r" (intdst)
			: "r" (INTCTRLADDR)
			: "memory");
	} while (intdst & 0b11);
	intdst = 0;
	__asm__ __volatile__ (
		"ldst %0, %1"
		: "+r" (intdst)
		: "r" (INTCTRLADDR)
		: "memory");
	return ((signed long)intdst >> 2);
}

static inline unsigned long hwdrvintctrl_ena (unsigned long idx, unsigned long en) {
	unsigned long intsrc;
	do {
		intsrc = ((idx<<3) | ((en&1)<<2) | 0b11);
		__asm__ __volatile__ (
			"ldst %0, %1"
			: "+r" (intsrc)
			: "r" (INTCTRLADDR)
			: "memory");
	} while (intsrc & 0b11);
	intsrc = 0;
	__asm__ __volatile__ (
		"ldst %0, %1"
		: "+r" (intsrc)
		: "r" (INTCTRLADDR)
		: "memory");
	return ((signed long)intsrc >> 2);
}

#endif /* HWDRVINTCTRL_H */
