// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

// Used for open().
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// Used for read(), write(), close().
#include <unistd.h>
// Used for poll().
#include <poll.h>
// Used for tty functions.
#include <termios.h>
// Used for nanosleep().
#include <time.h>
// Other includes.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

// The CPU must be little-endian because the variable storing
// the response from the debug device is written in little-endian.
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "little endian required"
#endif

#ifndef archint_t
#error missing -Darchint_t=intxx_t -Darchuint_t=uintxx_t
#endif

// The format of a request is as follow:
// |cmd: 3bits|arg: 5bits|
// The data returned is always (ARCHBITSZ/8) bytes in little-endian.
// The valid cmd values are:
// - DBGCMDSELECT:
//  Select the pu which must receive subsequent commands.
//  arg is the index of the pu.
//  No data is returned.
// - DBGCMDSTEP:
//  When arg is DBGARGSTEPDISABLE, the debug interface
//  gets disabled and the pu resumes executing instructions.
//  When arg is DBGARGSTEPSTOP, the debug interface gets
//  enabled and the pu stops executing instructions.
//  When arg is DBGARGSTEPONCE, the debug interface gets
//  enabled and the pu executes a single instruction.
//  When arg is DBGARGSTEPTILL, the debug interface gets
//  enabled and the pu executes instructions until
//  the address from the instruction pointer register is
//  the same as the value loaded through DBGCMDLOADIARG,
//  or until a command other than DBGCMDSTEP(DBGARGSTEPTILL)
//  is issued.
//  The instruction pointer register is returned if arg was
//  DBGARGSTEPONCE or DBGARGSTEPTILL, otherwise no data is returned.
// - DBGCMDGETOPCODE:
//  arg is meaningless.
//  The two bytes opcode of the next instruction to execute
//  are set in the two least significant bytes of the (ARCHBITSZ/8)
//  bytes returned.
// - DBGCMDGETIP:
//  arg is meaningless.
//  The value of the instruction pointer register is returned.
// - DBGCMDGETGPR:
//  The value of the GPR indexed by arg is returned.
// - DBGCMDSETGPR:
//  The value of the GPR indexed by arg is set using the value
//  loaded through DBGCMDLOADIARG.
//  No data is returned.
// - DBGCMDLOADIARG;
//  The 4lsb of arg are shifted in the 4msb of an internal register
//  used as argument; to load all ARCHBITSZ bits of that register,
//  this commands must be issued (ARCHBITSZ/4) times.
//  No data is returned.

// Enum used to name debug commands.
enum {
	DBGCMDSELECT		= 0,
	DBGCMDSTEP		= 1,
	DBGCMDGETOPCODE		= 2,
	DBGCMDGETIP		= 3,
	DBGCMDGETGPR		= 4,
	DBGCMDSETGPR		= 5,
	DBGCMDLOADIARG		= 6 };
// Enum used to name arguments of DBGCMDSTEP.
enum {
	DBGARGSTEPDISABLE	= 0,
	DBGARGSTEPSTOP		= 1,
	DBGARGSTEPONCE		= 2,
	DBGARGSTEPTILL		= 3 };

// Variables set to the index of the first
// and second gpr used by the opcode respectively.
unsigned gpr1idx, gpr2idx;
// Variable set to the number of gprs that decodeop() used.
unsigned gprcnt;
// Function used for slowing down between accesses to the debug device.
void dbgsleep (void);
// Print gpr value.
void gprval (unsigned idx);
// Decode opcode.
void decodeop (uint16_t opcode);
// Used to get terminal cursor position.
uint32_t getpos (void);
// Used to set terminal cursor position.
void setpos (uint32_t pos);
// Used to save terminal cursor position.
uint32_t savedpos;

unsigned long ttyfd;

struct termios /*term,*/ savedterm, origterm;

#define POLL_NFDS 2
struct pollfd pollfds[POLL_NFDS];
#define POLL_EVENTS_FLAGS (POLLIN /*| POLLPRI | POLLRDNORM | POLLRDBAND*/)

// TODO: Option to set tty speed ...
// TODO: usage output should also show commands implemented. ie:
// TODO: 	b: breakpoint
// TODO: 	c: continuous run
// TODO: 	n: next instruction stepping
// TODO: 	p: print all registers
// TODO: 	s: set gpr value TODO.
// TODO: ctlr+c to exit.

void quit (int status) {
	close(ttyfd);
	tcsetattr (0, TCSANOW, &origterm);
	exit (status);
}

int main (unsigned long argc, uint8_t** arg) {

	if (argc < 2) {
		fprintf (stderr, "usage: %s <path/to/tty>\n", arg[0]);
		return -1;
	}

	if ((ttyfd = open (arg[1], O_RDWR | O_NOCTTY | O_SYNC)) == -1) {
		fprintf (stderr, "could not open: %s\n", arg[1]);
		return -1;
	}

	struct termios ttyconfig;

	if (isatty(ttyfd)) {
		// Discard any data currently
		// buffered in the tty.
		tcflush(ttyfd, TCIOFLUSH);
		// Retrieve the current tty config.
		if (tcgetattr(ttyfd, &ttyconfig) == 0) {
			// Set the tty to raw mode.
			cfmakeraw(&ttyconfig);
			// Set the tty bitrate to 115200.
			if (cfsetispeed(&ttyconfig, B115200) == 0 &&
				cfsetospeed(&ttyconfig, B115200) == 0) {
				// A read() blocks for a max of 1s until
				// four characters have been received.
				ttyconfig.c_cc[VMIN] = 4;
				ttyconfig.c_cc[VTIME] = 10;
				// Apply the new tty config.
				if (tcsetattr(ttyfd, TCSANOW, &ttyconfig) == 0)
					goto ttyready;
				else fprintf(stderr, "applying tty config failed\n");
			} else fprintf(stderr, "setting bitrate failed\n");
		} else fprintf(stderr, "retrieving tty config failed\n");
	} else fprintf(stderr, "not a tty: %s\n", arg[1]);

	quit(1);

	ttyready:;

	// Discard any data currently buffered in STDIN.
	tcflush(0, TCIOFLUSH);

	tcgetattr (0, &origterm);
	savedterm = origterm;
	savedterm.c_lflag &= ~(ICANON|ECHO);
	tcsetattr (0, TCSANOW, &savedterm);

	pollfds[0].fd = 0;
	pollfds[0].events = POLL_EVENTS_FLAGS;
	pollfds[1].fd = ttyfd;
	pollfds[1].events = POLL_EVENTS_FLAGS;

	// ### On Linux printf() is buffered, preventing its output from
	// ### immediately showing on the screen; this disables that behavior.
	setvbuf(stdout, 0, _IONBF, 0);

	// Variable used to hold a request to send to the debug device.
	uint8_t rqst;
	// Variable used to hold a response from the debug device.
	// It is written in little-endian.
	archint_t rsp = -1;

	// Enable debugging on the pu with index 0.
	tcflush(ttyfd, TCIOFLUSH);
	rqst = ((DBGCMDSELECT << 5) | 0);
	if (write (ttyfd, &rqst, 1) != 1) {
		fprintf(stderr, "writing debug device failed: DBGCMDSELECT\n");
		quit(1);
	}
	dbgsleep();
	tcflush(ttyfd, TCIOFLUSH);
	rqst = ((DBGCMDSTEP << 5) | DBGARGSTEPSTOP);
	if (write (ttyfd, &rqst, 1) != 1) {
		fprintf(stderr, "writing debug device failed: DBGCMDSTEP|DBGARGSTEPSTOP\n");
		quit(1);
	}
	dbgsleep();

	gprcnt = 0;

	goto retrieveinstr;

	void *savedlabel = 0;

	while (1) {

		char buf[16] = {0};
		if (read (0, buf, 1) == 1) {
			unsigned i;
			static unsigned nlneeded = 0;
			switch (buf[0]) {
				case 'b': {
					if (nlneeded) {
						nlneeded = 0;
						write (1, "\n", 1);
					}
					setpos(savedpos);
					printf ("b ");
					for (i = 0; i < (sizeof(buf) -1);) {
						char c;
						if (read (0, &c, 1) == 1 && c != '\n') {
							switch (c) {
								case '0' ... '9':
								case 'a' ... 'f':
								case 'A' ... 'F':
								case 'x': case 'X':
									buf[i++] = c;
									write (1, &c, 1);
									break;
								case '\b':
								case 0x7f:
									if (i) {
										printf ("\033[D"); // Move cursor left one char.
										printf ("\033[K"); // Clear rest of line.
										--i; }}
						} else
							break;
					}
					buf[i] = 0;
					char *endptr;
					archuint_t breakpoint = strtol (buf, &endptr, 0);
					if (!buf[0] || *endptr != 0)
						printf (" < inv");
					else {
						savedlabel = 0;
						// Load the breakpoint address.
						for (i = 0; i < 8; ++i) {
							tcflush(ttyfd, TCIOFLUSH);
							rqst = ((DBGCMDLOADIARG << 5) | (breakpoint&0xf));
							if (write (ttyfd, &rqst, 1) != 1) {
								fprintf(stderr, "writing debug device failed: DBGCMDLOADIARG\n");
								quit(1);
							}
							dbgsleep();
							breakpoint >>= 4;
						}
						// Step until a break.
						tcflush(ttyfd, TCIOFLUSH);
						rqst = ((DBGCMDSTEP << 5) | DBGARGSTEPTILL);
						if (write (ttyfd, &rqst, 1) != 1) {
							fprintf(stderr, "writing debug device failed: DBGCMDSTEP|DBGARGSTEPTILL\n");
							quit(1);
						}
						dbgsleep();
						write (1, " ...", 4);
						dbgcmdsteptilldone:;
						while (poll ((struct pollfd *)pollfds, POLL_NFDS, -1) == -1) {
							if (errno == EINTR)
								continue;
							perror("poll()");
							quit(1);
						}
						if (pollfds[1].revents&POLL_EVENTS_FLAGS) {
							if (read (ttyfd, &rsp, sizeof(archuint_t)) != sizeof(archuint_t)) {
								fprintf(stderr, "reading debug device failed: DBGCMDSTEP|DBGARGSTEPTILL\n");
								quit(1);
							}
							dbgsleep();
							goto displayinsnregs;
						}
						savedlabel = &&dbgcmdsteptilldone;
					}
					continue; }
				case 'c': {
					if (nlneeded) {
						nlneeded = 0;
						write (1, "\n", 1);
					}
					setpos(savedpos);
					write (1, " ...", 4);
					// Disable debugging.
					tcflush(ttyfd, TCIOFLUSH);
					rqst = ((DBGCMDSTEP << 5) | DBGARGSTEPDISABLE);
					if (write (ttyfd, &rqst, 1) != 1) {
						fprintf(stderr, "writing debug device failed: DBGCMDSTEP|DBGARGSTEPDISABLE\n");
						quit(1);
					}
					dbgsleep();
					continue; };
				case 'n': {
					savedlabel = 0;
					// Step to the next instruction.
					tcflush(ttyfd, TCIOFLUSH);
					rqst = ((DBGCMDSTEP << 5) | DBGARGSTEPONCE);
					if (write (ttyfd, &rqst, 1) != 1) {
						fprintf(stderr, "writing debug device failed: DBGCMDSTEP|DBGARGSTEPONCE\n");
						quit(1);
					}
					dbgsleep();
					dbgcmdsteponcedone:;
					while (poll ((struct pollfd *)pollfds, POLL_NFDS, -1) == -1) {
						if (errno == EINTR)
							continue;
						perror("poll()");
						quit(1);
					}
					if (pollfds[1].revents&POLL_EVENTS_FLAGS) {
						if (read (ttyfd, &rsp, sizeof(archuint_t)) != sizeof(archuint_t)) {
							fprintf(stderr, "reading debug device failed: DBGCMDSTEP|DBGARGSTEPTILL\n");
							quit(1);
						}
						dbgsleep();
						if (nlneeded) {
							nlneeded = 0;
							write (1, "\n", 1);
						}
						goto displayinsnregs;
					}
					savedlabel = &&dbgcmdsteponcedone;
					continue;
					displayinsnregs:;
					setpos(savedpos);
					if (gprcnt > 0) {
						gprval(gpr1idx);
						if (gprcnt > 1) {
							printf(" ");
							gprval(gpr2idx);
						}
					}
					write (1, "\n", 1);
					break;
				}
				case 'p': {
					savedlabel = 0;
					write (1, "\n\t", 2);
					for (i = 0; i < 16; ++i)
						write (1, " ", 1), gprval(i);
					nlneeded = 1;
					continue;
				}
				//case 'q': write (1, "\n", 1); quit(1); // ### commented-out as ctrl+c works.
				case 's': {
					// TODO: DBGCMDSETGPR ...
				}
				default:
					if (savedlabel)
						goto *savedlabel;
					continue;
			}
		} else
			continue;

		// Retrieve the current value of the instruction pointer register.
		retrieveinstr:

		if (rsp == -1) {
			tcflush(ttyfd, TCIOFLUSH);
			rqst = (DBGCMDGETIP << 5);
			if (write (ttyfd, &rqst, 1) != 1) {
				fprintf(stderr, "writing debug device failed: DBGCMDGETIP\n");
				quit(1);
			}
			dbgsleep();
			if (read(ttyfd, &rsp, sizeof(archuint_t)) != sizeof(archuint_t)) {
				fprintf(stderr, "reading debug device failed: DBGCMDGETIP\n");
				quit(1);
			}
			dbgsleep();
		}
		printf("%08x: ", rsp);
		// Retrieve the opcode of the instruction at the address
		// currently in the instruction pointer register.
		tcflush(ttyfd, TCIOFLUSH);
		rqst = (DBGCMDGETOPCODE << 5);
		if (write (ttyfd, &rqst, 1) != 1) {
			fprintf(stderr, "writing debug device failed: DBGCMDGETOPCODE\n");
			quit(1);
		}
		dbgsleep();
		if (read(ttyfd, &rsp, sizeof(archuint_t)) != sizeof(archuint_t)) {
			fprintf(stderr, "reading debug device failed: DBGCMDGETOPCODE\n");
			quit(1);
		}
		dbgsleep();
		printf("%04x: ", rsp);
		decodeop((uint16_t)rsp);
		printf(": ");
		savedpos = getpos();
		rsp = -1;
	}

	quit(0);

	return 0;
}

void dbgsleep (void) {
	/*if(nanosleep(&((struct timespec){.tv_sec = 0, .tv_nsec = 1000000}), 0) < 0 ) {
		perror("nanosleep()");
		quit(1);
	}*/
}

void gprval (unsigned idx) {
	// Variable used to hold a request to send to the debug device.
	uint8_t rqst;
	// Variable used to hold a response from the debug device.
	// It is written in little-endian.
	archint_t rsp;
	tcflush(ttyfd, TCIOFLUSH);
	rqst = (DBGCMDGETGPR << 5 | idx);
	if (write (ttyfd, &rqst, 1) != 1) {
		fprintf(stderr, "writing debug device failed: DBGCMDGETGPR\n");
		quit(1);
	}
	dbgsleep();
	if (read(ttyfd, &rsp, sizeof(archuint_t)) != sizeof(archuint_t)) {
		fprintf(stderr, "reading debug device failed: DBGCMDGETGPR\n");
		quit(1);
	}
	dbgsleep();
	if (rsp < 0) {
		rsp = -rsp;
		printf("%%%d(-%u)", idx, rsp);
	} else
		printf("%%%d(%u)", idx, rsp);
}

void decodeop (uint16_t opcode) {
	gprcnt = 0;
	void decode1gpr (void) {
		gpr1idx = (opcode&0xf0)>>4;
		gprval(gpr1idx);
		gprcnt = 1;
	}
	void decode2gpr (void) {
		gpr1idx = (opcode&0xf0)>>4;
		gprval(gpr1idx); printf(" ");
		gpr2idx = opcode&0xf;
		gprval(gpr2idx);
		gprcnt = 2;
	}
	switch (opcode>>8) {
		case 0xb8:
			// Specification from the instruction set manual:
			// add %gpr1, %gpr2 |23|000|rrrr|rrrr|
			printf("add ");
			decode2gpr();
			break;
		case 0xb9:
			// Specification from the instruction set manual:
			// sub %gpr1, %gpr2 |23|001|rrrr|rrrr|
			printf("sub ");
			decode2gpr();
			break;
		case 0xca:
			// Specification from the instruction set manual:
			// mul %gpr1, %gpr2 |25|010|rrrr|rrrr|
			printf("mul ");
			decode2gpr();
			break;
		case 0xcb:
			// Specification from the instruction set manual:
			// mulh %gpr1, %gpr2 |25|011|rrrr|rrrr|
			printf("mulh ");
			decode2gpr();
			break;
		case 0xce:
			// Specification from the instruction set manual:
			// div %gpr1, %gpr2 |25|110|rrrr|rrrr|
			printf("div ");
			decode2gpr();
			break;
		case 0xcf:
			// Specification from the instruction set manual:
			// mod %gpr1, %gpr2 |25|111|rrrr|rrrr|
			printf("mod ");
			decode2gpr();
			break;
		case 0xc8:
			// Specification from the instruction set manual:
			// mulu %gpr1, %gpr2 |25|000|rrrr|rrrr|
			printf("mulu ");
			decode2gpr();
			break;
		case 0xc9:
			// Specification from the instruction set manual:
			// mulhu %gpr1, %gpr2 |25|001|rrrr|rrrr|
			printf("mulhu ");
			decode2gpr();
			break;
		case 0xcc:
			// Specification from the instruction set manual:
			// divu %gpr1, %gpr2 |25|100|rrrr|rrrr|
			printf("divu ");
			decode2gpr();
			break;
		case 0xcd:
			// Specification from the instruction set manual:
			// modu %gpr1, %gpr2 |25|101|rrrr|rrrr|
			printf("modu ");
			decode2gpr();
			break;
		case 0xd8:
			// Specification from the instruction set manual:
			// fadd %gpr1, %gpr2 |22|100|rrrr|rrrr|
			printf("fadd ");
			decode2gpr();
			break;
		case 0xd9:
			// Specification from the instruction set manual:
			// fsub %gpr1, %gpr2 |22|101|rrrr|rrrr|
			printf("fsub ");
			decode2gpr();
			break;
		case 0xda:
			// Specification from the instruction set manual:
			// fmul %gpr1, %gpr2 |22|110|rrrr|rrrr|
			printf("fmul ");
			decode2gpr();
			break;
		case 0xdb:
			// Specification from the instruction set manual:
			// fdiv %gpr1, %gpr2 |22|111|rrrr|rrrr|
			printf("fdiv ");
			decode2gpr();
			break;
		case 0xc3:
			// Specification from the instruction set manual:
			// and %gpr1, %gpr2 |24|011|rrrr|rrrr|
			printf("and ");
			decode2gpr();
			break;
		case 0xc4:
			// Specification from the instruction set manual:
			// or %gpr1, %gpr2 |24|100|rrrr|rrrr|
			printf("or ");
			decode2gpr();
			break;
		case 0xc5:
			// Specification from the instruction set manual:
			// xor %gpr1, %gpr2 |24|101|rrrr|rrrr|
			printf("xor ");
			decode2gpr();
			break;
		case 0xc6:
			// Specification from the instruction set manual:
			// not %gpr1, %gpr2 |24|110|rrrr|rrrr|
			printf("not ");
			decode2gpr();
			break;
		case 0xc7:
			// Specification from the instruction set manual:
			// cpy %gpr1, %gpr2 |24|111|rrrr|rrrr|
			printf("cpy ");
			decode2gpr();
			break;
		case 0xc0:
			// Specification from the instruction set manual:
			// sll %gpr1, %gpr2 |24|000|rrrr|rrrr|
			printf("sll ");
			decode2gpr();
			break;
		case 0xc1:
			// Specification from the instruction set manual:
			// srl %gpr1, %gpr2 |24|001|rrrr|rrrr|
			printf("srl ");
			decode2gpr();
			break;
		case 0xc2:
			// Specification from the instruction set manual:
			// sra %gpr1, %gpr2 |24|010|rrrr|rrrr|
			printf("sra ");
			decode2gpr();
			break;
		case 0xba:
			// Specification from the instruction set manual:
			// seq %gpr1, %gpr2 |23|010|rrrr|rrrr|
			printf("seq ");
			decode2gpr();
			break;
		case 0xbb:
			// Specification from the instruction set manual:
			// sne %gpr1, %gpr2 |23|011|rrrr|rrrr|
			printf("sne ");
			decode2gpr();
			break;
		case 0xbc:
			// Specification from the instruction set manual:
			// slt %gpr1, %gpr2 |23|100|rrrr|rrrr|
			printf("slt ");
			decode2gpr();
			break;
		case 0xbd:
			// Specification from the instruction set manual:
			// slte %gpr1, %gpr2 |23|101|rrrr|rrrr|
			printf("slte ");
			decode2gpr();
			break;
		case 0xbe:
			// Specification from the instruction set manual:
			// sltu %gpr1, %gpr2 |23|110|rrrr|rrrr|
			printf("sltu ");
			decode2gpr();
			break;
		case 0xbf:
			// Specification from the instruction set manual:
			// slteu %gpr1, %gpr2 |23|111|rrrr|rrrr|
			printf("slteu ");
			decode2gpr();
			break;
		case 0xb0:
			// Specification from the instruction set manual:
			// sgt %gpr1, %gpr2 |22|000|rrrr|rrrr|
			printf("sgt ");
			decode2gpr();
			break;
		case 0xb1:
			// Specification from the instruction set manual:
			// sgte %gpr1, %gpr2 |22|001|rrrr|rrrr|
			printf("sgte ");
			decode2gpr();
			break;
		case 0xb2:
			// Specification from the instruction set manual:
			// sgtu %gpr1, %gpr2 |22|010|rrrr|rrrr|
			printf("sgtu ");
			decode2gpr();
			break;
		case 0xb3:
			// Specification from the instruction set manual:
			// sgteu %gpr1, %gpr2 |22|011|rrrr|rrrr|
			printf("sgteu ");
			decode2gpr();
			break;
		case 0xd0:
			// Specification from the instruction set manual:
			// jz %gpr1 %gpr2 |26|000|rrrr|rrrr|
			printf("jz ");
			decode2gpr();
			break;
		case 0xd1:
			// Specification from the instruction set manual:
			// jnz %gpr1 %gpr2 |26|001|rrrr|rrrr|
			printf("jnz ");
			decode2gpr();
			break;
		case 0xd2:
			// Specification from the instruction set manual:
			// jl %gpr1 %gpr2 |26|010|rrrr|rrrr|
			printf("jl ");
			decode2gpr();
			break;
		case 0xad:
			// Specification from the instruction set manual:
			// rli16 %gpr, imm |21|101|rrrr|0000|
			//                 |iiiiiiiiiiiiiiii|
			printf("rli16 ");
			decode1gpr();
			break;
		case 0xae:
			// Specification from the instruction set manual:
			// rli32 %gpr, imm |21|110|rrrr|0000|
			//                 |iiiiiiiiiiiiiiii| 16 msb.
			//                 |iiiiiiiiiiiiiiii| 16 lsb.
			printf("rli32 ");
			decode1gpr();
			break;
		case 0xac:
			// Specification from the instruction set manual:
			// drli %gpr, imm |21|100|rrrr|0000|
			//                |iiiiiiiiiiiiiiii| 16 msb.
			//                |iiiiiiiiiiiiiiii| 16 lsb.
			printf("drli ");
			decode1gpr();
			break;
		case 0xa1:
			// Specification from the instruction set manual:
			// inc16 %gpr, imm	|20|001|rrrr|0000|
			// 			|iiiiiiiiiiiiiiii|
			printf("inc16 ");
			decode1gpr();
			break;
		case 0xa2:
			// Specification from the instruction set manual:
			// inc32 %gpr, imm	|20|010|rrrr|0000|
			// 			|iiiiiiiiiiiiiiii| 16 msb.
			// 			|iiiiiiiiiiiiiiii| 16 lsb.
			printf("inc32 ");
			decode1gpr();
			break;
		case 0xa9:
			// Specification from the instruction set manual:
			// li16 %gpr, imm	|21|001|rrrr|0000|
			// 			|iiiiiiiiiiiiiiii| 16 msb.
			printf("li16 ");
			decode1gpr();
			break;
		case 0xaa:
			// Specification from the instruction set manual:
			// li32 %gpr, imm	|21|010|rrrr|0000|
			// 			|iiiiiiiiiiiiiiii| 16 msb.
			// 			|iiiiiiiiiiiiiiii| 16 lsb.
			printf("li32 ");
			decode1gpr();
			break;
		case 0xf4:
			// Specification from the instruction set manual:
			// ld8 %gpr1, %gpr2 |30|100|rrrr|rrrr|
			printf("ld8 ");
			decode2gpr();
			break;
		case 0xf5:
			// Specification from the instruction set manual:
			// ld16 %gpr1, %gpr2 |30|101|rrrr|rrrr|
			printf("ld16 ");
			decode2gpr();
			break;
		case 0xf6:
			// Specification from the instruction set manual:
			// ld32 %gpr1, %gpr2 |30|110|rrrr|rrrr|
			printf("ld32 ");
			decode2gpr();
			break;
		case 0xf0:
			// Specification from the instruction set manual:
			// st8 %gpr1, %gpr2 |30|000|rrrr|rrrr|
			printf("st8 ");
			decode2gpr();
			break;
		case 0xf1:
			// Specification from the instruction set manual:
			// st16 %gpr1, %gpr2 |30|001|rrrr|rrrr|
			printf("st16 ");
			decode2gpr();
			break;
		case 0xf2:
			// Specification from the instruction set manual:
			// st32 %gpr1, %gpr2 |30|010|rrrr|rrrr|
			printf("st32 ");
			decode2gpr();
			break;
		case 0xf8:
			// Specification from the instruction set manual:
			// ldst8 %gpr1, %gpr2 |31|000|rrrr|rrrr|
			printf("ldst8 ");
			decode2gpr();
			break;
		case 0xf9:
			// Specification from the instruction set manual:
			// ldst16 %gpr1, %gpr2 |31|001|rrrr|rrrr|
			printf("ldst16 ");
			decode2gpr();
			break;
		case 0xfa:
			// Specification from the instruction set manual:
			// ldst32 %gpr1, %gpr2 |31|010|rrrr|rrrr|
			printf("ldst32 ");
			decode2gpr();
			break;
		case 0xfc:
			// Specification from the instruction set manual:
			// cldst8 %gpr1, %gpr2 |31|100|rrrr|rrrr|
			printf("cldst8 ");
			decode2gpr();
			break;
		case 0xfd:
			// Specification from the instruction set manual:
			// cldst16 %gpr1, %gpr2 |31|101|rrrr|rrrr|
			printf("cldst16 ");
			decode2gpr();
			break;
		case 0xfe:
			// Specification from the instruction set manual:
			// cldst32 %gpr1, %gpr2 |31|110|rrrr|rrrr|
			printf("cldst32 ");
			decode2gpr();
			break;
		case 0x00:
			// Specification from the instruction set manual:
			// sysret |0|000|0000|0000|
			printf("sysret ");
			break;
		case 0x01:
			// Specification from the instruction set manual:
			// syscall |0|001|0000|0000|
			printf("syscall ");
			break;
		case 0x02:
			// Specification from the instruction set manual:
			// brk |0|010|0000|0000|
			printf("brk ");
			break;
		case 0x03:
			// Specification from the instruction set manual:
			// halt |0|011|0000|0000|
			printf("halt ");
			break;
		case 0x04:
			// Specification from the instruction set manual:
			// icacherst |0|100|0000|0000|
			printf("icacherst ");
			break;
		case 0x05:
			// Specification from the instruction set manual:
			// dcacherst |0|101|0000|0000|
			printf("dcacherst ");
			break;
		case 0x07:
			// Specification from the instruction set manual:
			// ksysret |0|111|0000|0000|
			printf("ksysret ");
			break;
		case 0x39:
			// Specification from the instruction set manual:
			// setksl %gpr |7|001|rrrr|0000|
			printf("setksl ");
			decode1gpr();
			break;
		case 0x3c:
			// Specification from the instruction set manual:
			// setasid %gpr |7|100|rrrr|0000|
			printf("setasid ");
			decode1gpr();
			break;
		case 0x3d:
			// Specification from the instruction set manual:
			// setuip %gpr |7|101|rrrr|0000|
			printf("setuip ");
			decode1gpr();
			break;
		case 0x3e:
			// Specification from the instruction set manual:
			// setflags %gpr |7|110|rrrr|0000|
			printf("setflags ");
			decode1gpr();
			break;
		case 0x3f:
			// Specification from the instruction set manual:
			// settimer %gpr |7|111|rrrr|0000|
			printf("settimer ");
			decode1gpr();
			break;
		case 0x3a:
			// Specification from the instruction set manual:
			// settlb %gpr1, %gpr2 |7|010|rrrr|rrrr|
			printf("settlb ");
			decode2gpr();
			break;
		case 0x3b:
			// Specification from the instruction set manual:
			// clrtlb %gpr1, %gpr2 |7|011|rrrr|rrrr|
			printf("clrtlb ");
			decode2gpr();
			break;
		case 0x79:
			// Specification from the instruction set manual:
			// setkgpr %gpr1 %gpr2 |15|001|rrrr|rrrr|
			printf("setkgpr ");
			decode2gpr();
			break;
		case 0x7a:
			// Specification from the instruction set manual:
			// setugpr %gpr1 %gpr2 |15|010|rrrr|rrrr|
			printf("setugpr ");
			decode2gpr();
			break;
		case 0x7b:
			// Specification from the instruction set manual:
			// setgpr %gpr1 %gpr2 |15|011|rrrr|rrrr|
			printf("setgpr ");
			decode2gpr();
			break;
		case 0x28:
			// Specification from the instruction set manual:
			// getsysopcode %gpr |5|000|rrrr|0000|
			printf("getsysopcode ");
			decode1gpr();
			break;
		case 0x29:
			// Specification from the instruction set manual:
			// getuip %gpr |5|001|rrrr|0000|
			printf("getuip ");
			decode1gpr();
			break;
		case 0x2a:
			// Specification from the instruction set manual:
			// getfaultaddr %gpr |5|010|rrrr|0000|
			printf("getfaultaddr ");
			decode1gpr();
			break;
		case 0x2b:
			// Specification from the instruction set manual:
			// getfaultreason %gpr |5|011|rrrr|0000|
			printf("getfaultreason ");
			decode1gpr();
			break;
		case 0x2c:
			// Specification from the instruction set manual:
			// getclkcyclecnt %gpr |5|100|rrrr|0000|
			printf("getclkcyclecnt ");
			decode1gpr();
			break;
		case 0x2d:
			// Specification from the instruction set manual:
			// getclkcyclecnth %gpr |5|101|rrrr|0000|
			printf("getclkcyclecnth ");
			decode1gpr();
			break;
		case 0x2e:
			// Specification from the instruction set manual:
			// gettlbsize %gpr |5|110|rrrr|0000|
			printf("gettlbsize ");
			decode1gpr();
			break;
		case 0x2f:
			// Specification from the instruction set manual:
			// geticachesize %gpr |5|111|rrrr|0000|
			printf("geticachesize ");
			decode1gpr();
			break;
		case 0x10:
			// Specification from the instruction set manual:
			// getcoreid %gpr |2|000|rrrr|0000|
			printf("getcoreid ");
			decode1gpr();
			break;
		case 0x11:
			// Specification from the instruction set manual:
			// getclkfreq %gpr |2|001|rrrr|0000|
			printf("getclkfreq ");
			decode1gpr();
			break;
		case 0x12:
			// Specification from the instruction set manual:
			// getdcachesize %gpr |2|010|rrrr|0000|
			printf("getdcachesize ");
			decode1gpr();
			break;
		case 0x13:
			// Specification from the instruction set manual:
			// gettlb %gpr1, %gpr2 |2|011|rrrr|rrrr|
			printf("gettlb ");
			decode2gpr();
			break;
		default: {
			if (opcode == 0b1001000000000000)
				printf("preempt");
			else {
				do {
					unsigned long x = ((opcode>>12)&0xf);
					if (x == 0b1000) {
						// Specification from the instruction set manual:
						// li8 %gpr, imm |1000|iiii|rrrr|iiii|
						printf("li8 ");
					} else if (x == 0b1001) {
						// Specification from the instruction set manual:
						// inc8 %gpr, imm |1001|iiii|rrrr|iiii|
						printf("inc8 ");
					} else if (x == 0b1110) {
						// Specification from the instruction set manual:
						// rli8 %gpr, imm |1110|iiii|rrrr|iiii|
						printf("rli8 ");
					} else {
						printf("????");
						break;
					}
					decode1gpr();
					printf(" ");
					int8_t n = ((opcode & 0xf) + (((opcode>>8) & 0xf) << 4));
					if (n < 0) {
						n = -n;
						printf("-");
					}
					printf("%d", n);
				} while (0);
			}
			break;
		}
	}
}

uint32_t getpos (void) {

	//term = origterm;
	//term.c_lflag &= ~(ICANON|ECHO);
	//tcsetattr (0, TCSANOW, &term);

	printf ("\033[6n");

	unsigned i; char c, buf[16] = {0};
	for (i = 0, c = 0; (c != 'R' && i < sizeof(buf)); ++i) {
		if (read (0, &c, 1) != 1) {
			tcsetattr (0, TCSANOW, &savedterm);
			fprintf(stderr, "getpos: error reading response!\n");
			return 0;
		}
		buf[i] = c;
	}
	if (c != 'R') {
		tcsetattr (0, TCSANOW, &savedterm);
		return 0;
	}

	union {
		uint16_t pos[2];
		uint32_t val;
	} ret = { .val = 0 };

	unsigned pow;
	// Just scanf (" \033[%d;%dR", &x, &y) could have been used; but it is not working.
	for (i -= 2, pow = 1; buf[i] != ';'; --i, pow *= 10)
		ret.pos[0] = ret.pos[0] + ((buf[i] - '0') * pow);
	for(--i, pow = 1; buf[i] != '['; --i, pow *= 10)
		ret.pos[1] = ret.pos[1] + ((buf[i] - '0') * pow);

	ret.pos[0] -= 1; // Adjustment to take into account escape code cursor movements.

	//tcsetattr (0, TCSANOW, &savedterm);

	//printf ("x: %d; y: %d ... ", ret.pos[0], ret.pos[1]);

	return ret.val;
}

void setpos (uint32_t pos) {

	//term = origterm;
	//term.c_lflag &= ~(ICANON|ECHO);
	//tcsetattr (0, TCSANOW, &term);

	union {
		uint16_t pos[2];
		uint32_t val;
	} arg = { .val = pos };

	printf ("\033[%d;%dH", arg.pos[1], arg.pos[0]+1);
	printf ("\033[K"); // Clear rest of line.

	//tcsetattr (0, TCSANOW, &savedterm);
}
