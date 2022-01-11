// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef HWDRVDEVTBL_H
#define HWDRVDEVTBL_H

// Scanning through the device table is to
// stop at the entry with DeviceMapSz null.

// Structure describing a device table entry.
typedef struct {
	unsigned long id;
	struct {
		unsigned long useintr	:1;
		unsigned long _		:1;
		unsigned long mapsz	:((8*sizeof(unsigned long))-(
			(sizeof(unsigned long) == 2) ? 1 :
			(sizeof(unsigned long) == 4) ? 2 : 3));
	};
} devtblentry;

// Structure used by hwdrvdevtbl_find().
typedef struct {
	devtblentry* e; // Address of device table entry.
	unsigned long id; // DeviceID of the device.
	void* addr; // Address where the device is mapped in memory.
	unsigned long mapsz; // Count of sizeof(unsigned long) that the device
	                     // mapping occupies in the address space.
	signed long intridx; // Index of the device within the interrupt controller.
} hwdrvdevtbl;

// Search the device table for a device.
// The fields dev->e and dev->id must be properly set before calling this function.
// dev->e must be set to the device table entry address where to begin matching;
// when null, a match begins from the conventional first device table entry address;
// when non-null, all other fields of *dev must be valid.
// If a device is found, it gets returned in the argument *dev, otherwise the fields
// dev->mapsz and dev->intridx get set to null and a negative value respectively.
// When a device found does not use interrupt, its field dev->intridx is a negative value.
// If cb is non-null, for every device traversed, the function it points-to gets called.
// If cb == -1, hwdrvdevtbl_find() returns in *dev, the following device.
// Scanning through the device table stops at the entry with DeviceMapSz null or
// if cb is non-null and returns non-null.
static void hwdrvdevtbl_find (hwdrvdevtbl *dev, unsigned long (*cb)(hwdrvdevtbl *)) {

	devtblentry* d;
	hwdrvdevtbl ddev;
	unsigned long intridx;

	if (dev->e) {

		ddev = *dev;
		d = ddev.e;
		intridx = ((ddev.intridx >= 0) ? (ddev.intridx +1) : -ddev.intridx);
		goto resumescan;

	} else {

		d = (devtblentry *)0x200; // By convention, the device table is located at 0x200.

		// Interrupt 0 is always in used and must be assumed
		// assigned to a block device mapped at address 0x0.

		intridx = 1;

		ddev.e = d;
		ddev.id = d->id;
		ddev.addr = (void *)0;
		ddev.mapsz = d->mapsz;
		ddev.intridx = 0;
	}

	while (1) {
		if (!d->mapsz || (cb && cb != (void *)-1 && cb(&ddev))) {
			dev->e = ddev.e;
			dev->addr = ddev.addr;
			dev->mapsz = 0;
			dev->intridx = -intridx;
			return;
		}
		if ((!dev->e || d >= dev->e) && (cb == (void *)-1 || d->id == dev->id)) {
			*dev = ddev;
			return;
		}
		resumescan:
		ddev.addr += ((d++)->mapsz * sizeof(unsigned long));
		ddev.e = d;
		ddev.id = d->id;
		ddev.mapsz = d->mapsz;
		ddev.intridx = (d->useintr ? ((intridx += 1) -1) : -intridx);
	}
}

#endif /* HWDRVDEVTBL_H */
