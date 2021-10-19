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
	// Address of device table entry.
	devtblentry* e;
	// DeviceID of the device.
	unsigned long id;
	// Address where the device is mapped in memory.
	void* addr;
	// Count of sizeof(unsigned long) that the device
	// mapping occupies in the address space.
	unsigned long mapsz;
	// Index of the device within the interrupt controller.
	unsigned long intridx;
} hwdrvdevtbl;

// Search the device table for a device.
// The fields dev->e and dev->id must be properly set before calling this function.
// dev->e must be set to the device table entry address where to begin matching;
// when null, a match begins from the conventional first device table entry address.
// dev->id must be set to the DeviceID of the device to find.
// If a device is found, it gets returned in the argument *dev, otherwise the fields
// dev->mapsz and dev->intridx of the argument *dev get set to null and -1 respectively.
// If a device found do not use interrupt, its field dev->intridx get set to -1.
static void hwdrvdevtbl_find (hwdrvdevtbl *dev) {

	devtblentry* d = (devtblentry *)0x200; // By convention, the device table is located at 0x200.

	hwdrvdevtbl ddev;

	unsigned long intridx = 0;

	ddev.e = d;
	ddev.id = d->id;
	ddev.addr = (void *)0;
	ddev.mapsz = d->mapsz;
	ddev.intridx = ((d->useintr ? (intridx += d->useintr) : 0) -1);

	while (1) {
		if (!d->mapsz) {
			// Stop scanning through the devtable
			// at the entry with DeviceMapSz null.
			dev->mapsz = 0;
			dev->intridx = -1;
			return;
		}
		if ((!dev->e || d >= dev->e) && d->id == dev->id) {
			*dev = ddev;
			return;
		}
		ddev.addr += ((d++)->mapsz * sizeof(unsigned long));
		ddev.e = d;
		ddev.id = d->id;
		ddev.mapsz = d->mapsz;
		ddev.intridx = ((d->useintr ? (intridx += d->useintr) : 0) -1);
	}
}

#endif /* HWDRVDEVTBL_H */
