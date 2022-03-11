// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef HEXDUMP_H
#define HEXDUMP_H

// Function which write out byte at a time,
// an hexdump of the memory region given by
// the arguments mem and len.
static void hexdump (void *mem, unsigned long len) {

	enum {
		// Number of bytes to display per line.
		COLUMNCOUNT = 16 };

	unsigned long i = 0;

	while (1) {

		if (!(i % COLUMNCOUNT)) {
			// Print offset.
			puts_hex(i); puts("  ");
		}

		unsigned long j = i;

		unsigned long k = j + COLUMNCOUNT;

		do { // Print hex data.
			if (j < len) {
				if ((j != i) && !(j%(COLUMNCOUNT/2)))
					putchar(' ');
				puts_hex(((unsigned char*)mem)[j]); putchar(' ');
			} else
				puts("   ");
		} while (++j < k);

		// Writeout ASCII dump.

		// Buffer to be used for comparison in order to catch duplicate lines.
		unsigned char buf[COLUMNCOUNT];

		puts(" |");

		j = i;

		do {
			unsigned char c = ((unsigned char*)mem)[j];
			// Writeout a dot if the byte is not printable.
			putchar((c >= 0x20 && c <= 0x7e) ? c : '.');
			// Save the byte for later comparison.
			buf[j % COLUMNCOUNT] = c;
		} while (++j < k && j < len);

		puts("|\n");

		j = 1;

		checkifdone:

		if ((i += COLUMNCOUNT) < len) {
			if ((len - i) > COLUMNCOUNT &&
				(uintcmp((void*)&buf, mem+i, COLUMNCOUNT) ==
					((void*)&buf+(COLUMNCOUNT*sizeof(unsigned long))))) {
				if (j) {
					puts("*\n");
					j = 0;
				}
				goto checkifdone;
			}
		} else
			break;
	}
}

#endif /* HEXDUMP_H */
