// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef HEXDUMP_H
#define HEXDUMP_H

//int putchar (int);
//void *uintcmp (void *dst, void *src, unsigned long cnt);

// Function which write out byte at a time,
// an hexdump of the memory region given by
// the arguments mem and len.
static void hexdump (void *mem, unsigned long len) {

	unsigned char hex (unsigned char c) {
		c = (c+((c>=10)?('a'-10):'0'));
		return c;
	}

	void printhex (unsigned long n) {
		for (unsigned i = 0; i < (2*sizeof(n)); ++i)
			putchar(hex((n>>(((8*sizeof(n))-4)-(i*4)))&0xf));
	}

	void printu8hex (unsigned char n) {
		for (unsigned i = 0; i < (2*sizeof(n)); ++i)
			putchar(hex((n>>(((8*sizeof(n))-4)-(i*4)))&0xf));
	}

	// Take a string as argument, and pass each byte to putchar().
	void printstr (unsigned char *str) {
		unsigned char c;
		while (c = *str) {
			putchar(c);
			++str;
		}
	}

	enum {
		// Number of bytes to display per line.
		COLUMNCOUNT = 16 };

	unsigned long i = 0;

	while (1) {

		if (!(i % COLUMNCOUNT)) {
			// Print offset.
			printhex(i); printstr("  ");
		}

		unsigned long j = i;

		unsigned long k = j + COLUMNCOUNT;

		do { // Print hex data.
			if (j < len) {
				if ((j != i) && !(j%(COLUMNCOUNT/2)))
					putchar(' ');
				printu8hex(((unsigned char*)mem)[j]); putchar(' ');
			} else
				printstr("   ");
		} while (++j < k);

		// Writeout ASCII dump.

		// Buffer to be used for comparison in order to catch duplicate lines.
		unsigned char buf[COLUMNCOUNT];

		printstr(" |");

		j = i;

		do {
			unsigned char c = ((unsigned char*)mem)[j];
			// Writeout a dot if the byte is not printable.
			putchar((c >= 0x20 && c <= 0x7e) ? c : '.');
			// Save the byte for later comparison.
			buf[j % COLUMNCOUNT] = c;
		} while (++j < k && j < len);

		printstr("|\n");

		j = 1;

		checkifdone:

		if ((i += COLUMNCOUNT) < len) {
			if ((len - i) > COLUMNCOUNT &&
				(uintcmp((void*)&buf, mem+i, COLUMNCOUNT) ==
					((void*)&buf+(COLUMNCOUNT*sizeof(unsigned long))))) {
				if (j) {
					printstr("*\n");
					j = 0;
				}
				goto checkifdone;
			}
		} else
			break;
	}
}

#endif /* HEXDUMP_H */
