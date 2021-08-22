// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#define STACKSZ		128 /* computed from -fstack-usage outputs and sizeof(savedkctx) */
#define UARTADDR	(0x0ff8 /* By convention, the first UART is located at 0x0ff8 */)
#define UARTBAUD	115200
#define BLKDEVADDR	(0x0 /* By convention, the first block device is located at 0x0 */)
#define DEVTBLADDR	(0x200 /* By convention, the device table is located at 0x200 */)
#define RAMDEVADDR	(0x1000 /* By convention, the first RAM device is located at 0x1000 */)
#define KERNELADDR	0x4000 /* must match corresponding constant in the kernel source-code */
#define KERNPART	2
#define CLDSTMUTEXCNT	8 /* the greater this value, the least likely threads will contend */

#define MAXCORECNT 1

#define BIOS_FD_STDIN		4
#define BIOS_FD_STDOUT		1
#define BIOS_FD_STDERR		2
#define BIOS_FD_STORAGEDEV	5
#define BIOS_FD_NETWORKDEV	6
#define BIOS_FD_INTCTRLDEV	7

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// Must match linux/include/uapi/asm-generic/unistd.h .
#define __NR_syscalls		441
#define __NR_settls		(__NR_syscalls+0)
#define __NR_gettls		(__NR_syscalls+1)
#define __NR_PU32_syscalls_start (__NR_syscalls+2)
#define __NR_lseek		(__NR_PU32_syscalls_start+0)
#define __NR_exit		93
#define __NR_exit_group		94
#define __NR_openat		56
#define __NR_close		57
#define __NR_read		63
#define __NR_write		64
#define __NR_writev		66
#define __NR_unlinkat		35
#define __NR_linkat		37
#define __NR_readlinkat		78
#define __NR_fstat64		80
#define __NR_getuid		174
#define __NR_geteuid		175
#define __NR_getgid		176
#define __NR_getegid		177
#define __NR_getpid		172
#define __NR_kill		129
#define __NR_brk		214
#define __NR_mmap2		222
#define __NR_chdir		49
#define __NR_fchmodat 		53
#define __NR_ioctl		29
