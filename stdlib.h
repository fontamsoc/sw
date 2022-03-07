// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef STDLIB_H
#define STDLIB_H

static char *itoa (unsigned n, char *s, unsigned b) {

	unsigned i = 0;

	// Handle 0 explicitly, otherwise empty string is printed for 0.
	if (n == 0) {
		s[i++] = '0';
		s[i] = '\0';
		return s;
	}

	unsigned isneg = 0;

	// In standard itoa(), negative nbers are handled only
	// with base 10. Otherwise nbers are considered unsigned.
	if (n < 0 && b == 10) {
		isneg = 1;
		n = -n;
	}

	#define ITOA_BUFSZ ((8*sizeof(void *)))
	char buf[ITOA_BUFSZ];

	i = ITOA_BUFSZ;

	// Process individual digits.
	while (n != 0) {
		unsigned r = (n % b);
		buf[--i] = ((r > 9) ? ((r-10) + 'a') : (r + '0'));
		n /= b;
	}

	// If nber is negative, append '-'.
	if (isneg)
		buf[--i] = '-';

	unsigned j = 0;
	while (i < ITOA_BUFSZ)
		s[j] = buf[i], ++i, j++;

	s[j] = '\0'; // Append string terminator.

	return s;
}

#endif /* STDLIB_H */
