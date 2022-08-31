#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-only
# (c) William Fonkou Tambe

#TO_FIX_AT_INSTALL#prefix=""
[ -z "${prefix}" ] && {
	echo error: $(basename ${0}) was not correctly installed
	exit 1
}

function usage {
	cat << __EOF__
usage: $(basename ${0}) [options] <image> [<misc+files>]

	Utility that creates a standard BIOS partition table on <image>
	writting it with boot-code bios-code kernel-code and/or rootfs-data.
	[<misc+files>] are files to save in the FAT32 partition.

options:

-b <bios>
	bios-code to write in <image> second partition.
	If not specified, default bios is used.

-k <kernel>
	kernel-code to write in <image> third partition.
	If not specified, nothing is used.

-l <loader>
	loader-code to write in <image> first 446 bytes.
	If not specified, default loader is used.

-r <rootfs>
	rootfs-data to write in <image> fourth partition.
	If not specified, nothing is used.

-t <target>
	target-name for which image is specific to.
	If not specified, target specific data are excluded.
__EOF__
}

while [ "$1" != "" ]; do
	case $1 in
		-b)
			shift
			opt_bios="$1"
			;;
		-k)
			shift
			opt_kernel="$1"
			;;
		-l)
			shift
			opt_loader="$1"
			;;
		-r)
			shift
			opt_rootfs="$1"
			;;
		-t)
			shift
			opt_target="$1"
			;;
		-*)
			echo error: invalid option: $1
			usage
			exit 1
			;;
		*)
			if [ -z "${blkdev}" ]; then
				blkdev="$1"
			else
				misc_files="${misc_files} $1"
			fi
	esac
	shift
done

[ -z "${blkdev}" ] && {
	echo error: image file missing
	usage
	exit 1
}

opt_loader=${opt_loader:-"${prefix}/lib/socloader.bin"}
opt_bios=${opt_bios:-"${prefix}/lib/socbios.bin"}

[ -f "${opt_bios}" ] || {
	echo error: bios file missing
	exit 1
}

[ -n "${opt_kernel}" -a ! -f "${opt_kernel}" ] && {
	echo error: kernel file missing
	exit 1
}

[ -f "${opt_loader}" ] || {
	echo error: loader file missing
	exit 1
}

[ -n "${opt_rootfs}" -a ! -f "${opt_rootfs}" ] && {
	echo error: rootfs file missing
	exit 1
}

if [ -n "${opt_target}" ]; then
	# ### Disabled for now.
	#if [ "${opt_target}" != "nexys4ddr" ]; then
		echo error: unsupported target
		exit 1
	#fi
fi

bios_sz=$(($(stat -L -c%s "${opt_bios}")+0))
kernel_sz=$(($(stat -L -c%s "${opt_kernel}" 2>/dev/null)+0))
loader_sz=$(($(stat -L -c%s "${opt_loader}")+0))
rootfs_sz=$(($(stat -L -c%s "${opt_rootfs}" 2>/dev/null)+0))
kernel_sz=$(((${kernel_sz}>0)?${kernel_sz}:1))
bios_blksz=$(((${bios_sz}/512)+((${bios_sz}%512)>0)))
kernel_blksz=$(((${kernel_sz}/512)+((${kernel_sz}%512)>0)))
loader_blksz=$(((${loader_sz}/512)+((${loader_sz}%512)>0)))
rootfs_blksz=$(((${rootfs_sz}/512)+((${rootfs_sz}%512)>0)))
kernel_sz=$(((${kernel_sz}<1)?1:${kernel_sz}))
# ### When last partition is less than 3 blocks,
# ### sfdisk throws "All space in use".
rootfs_blksz=$(((${rootfs_blksz}<3)?3:${rootfs_blksz}))

[ ${loader_sz} -gt 446 ] && {
	echo error: loader-code size is greater than 446 bytes
	exit 1
}

PARKPUSZ=24
BIOSMAXSZ=$(((7*0x1000)-${PARKPUSZ}))
# Note that -${PARKPUSZ} takes into account parkpu expected by multipu at (KERNELADDR - PARKPUSZ).
[ ${bios_sz} -gt ${BIOSMAXSZ} ] && {
	echo error: bios-code size is greater than ${BIOSMAXSZ} bytes
	exit 1
}

if [ "${opt_target}" = "nexys4ddr" ]; then
	firstblkidx=3
else
	firstblkidx=1
fi

set -x

# 32MB to be used by the FAT32 partition.
fatpart_blksz=$(((32*1024*1024)/512))

total_blksz=$((${firstblkidx}+${fatpart_blksz}+${bios_blksz}+${kernel_blksz}+${rootfs_blksz}))
dd if=/dev/zero of="${blkdev}" bs=1M count=$(((${total_blksz}/2048)+((${total_blksz}%2048)>0))) status=progress

unset isloopdev
[ -b "${blkdev}" ] || {
	initial_blkdev=${blkdev}
	blkdev=$(losetup --show -P -f "${blkdev}")
	[ -z "${blkdev}" ] && {
		echo error: losetup failed
		exit 1
	}
	isloopdev="p"
}

function rmloopdev {
	rm -rf ${blkdev}p*
	losetup -d "${blkdev}"
}

# trap ctrl-c and call ctrl_c()
trap ctrl_c INT

function ctrl_c {
	[ -n "${isloopdev}" ] && rmloopdev
	[ -n "${tmpdir}" ] && rm -rf "${tmpdir}"
	exit 1
}

sfdisk --no-tell-kernel "${blkdev}" << __EOF__ ||
# Partition type values taken from:
# https://en.wikipedia.org/wiki/Partition_type
label: dos
unit: sectors
grain: 512
# First partition is for storing FPGA bitstream.
${firstblkidx}, ${fatpart_blksz}, e
$((${firstblkidx}+${fatpart_blksz})), ${bios_blksz}, f8
$((${firstblkidx}+${fatpart_blksz}+${bios_blksz})), ${kernel_blksz}, 8a
$((${firstblkidx}+${fatpart_blksz}+${bios_blksz}+${kernel_blksz})), ${rootfs_blksz}, 83
__EOF__
	{
	echo error: sfdisk failed
	[ -n "${isloopdev}" ] && rmloopdev
	exit 1
}

# Create loop-device partitions.
[ -n "${isloopdev}" ] && {
	losetup -d "${blkdev}"
	blkdev=$(losetup --show -P -f "${initial_blkdev}")
	[ -z "${blkdev}" ] && {
		echo error: losetup failed
		exit 1
	}
	blkparts=$(lsblk -rn -o 'MAJ:MIN' "${blkdev}" | tail -n +2)
	cntr=1
	for i in ${blkparts}; do
		maj=$(echo $i | cut -d: -f1)
		min=$(echo $i | cut -d: -f2)
		blkpart="${blkdev}p${cntr}"
		rm -rf "${blkpart}"
		mknod ${blkpart} b ${maj} ${min}
		cntr=$((cntr + 1))
	done
}

partprobe "${blkdev}"

# Check that partitions block devices exist under /dev/ .
[	-b "${blkdev}${isloopdev}1" -a \
	-b "${blkdev}${isloopdev}2" -a \
	-b "${blkdev}${isloopdev}3" -a \
	-b "${blkdev}${isloopdev}4" ] || {
	echo error: partitions block devices missing from /dev/
	[ -n "${isloopdev}" ] && rmloopdev
	exit 1
}

if [ "${opt_target}" = "nexys4ddr" ]; then
	nexys4ddr0_opt_loader="${prefix}/lib/0.nexys4ddr.socloader.bin"
	nexys4ddr1_opt_loader="${prefix}/lib/1.nexys4ddr.socloader.bin"
	[ -f "${nexys4ddr0_opt_loader}" -a -f "${nexys4ddr1_opt_loader}" ] || {
		echo error: nexys4ddr loader files missing
		exit 1
	}
	dd if="${nexys4ddr0_opt_loader}" of="${blkdev}" bs=1                 count=446 oflag=sync status=progress
	dd if="${nexys4ddr1_opt_loader}" of="${blkdev}" bs=1 seek=$((512*1)) count=446 oflag=sync status=progress
	dd if="${opt_loader}"            of="${blkdev}" bs=1 seek=$((512*2)) count=446 oflag=sync status=progress
else
	dd if="${opt_loader}" of="${blkdev}" bs=1 count=446 oflag=sync status=progress
fi

mkfs.fat -v -f1 ${blkdev}${isloopdev}1 || {
	echo error: mkfs.fat failed
	rm -rf ${tmpdir}
	[ -n "${isloopdev}" ] && rmloopdev
	exit 1
}

[ -n "${misc_files}" ] && {
	tmpdir="$(mktemp -d)"
	[ -z "${tmpdir}" ] && {
		echo error: mktemp failed
		[ -n "${isloopdev}" ] && rmloopdev
		exit 1
	}
	mount -t vfat ${blkdev}${isloopdev}1 ${tmpdir} || {
		echo error: mount failed
		rm -rf ${tmpdir}
		[ -n "${isloopdev}" ] && rmloopdev
		exit 1
	}
	cp -r ${misc_files} ${tmpdir}/ || {
		echo error: cpy failed
		umount ${tmpdir}
		rm -rf ${tmpdir}
		[ -n "${isloopdev}" ] && rmloopdev
		exit 1
	}
	sync "${blkdev}${isloopdev}1"
	umount ${tmpdir}
	rm -rf ${tmpdir}
}
dd if="${opt_bios}" of="${blkdev}${isloopdev}2" bs=1M oflag=sync status=progress
[ -f "${opt_kernel}" ] && dd if="${opt_kernel}" bs=1M of="${blkdev}${isloopdev}3" oflag=sync status=progress
[ -f "${opt_rootfs}" ] && dd if="${opt_rootfs}" bs=1M of="${blkdev}${isloopdev}4" oflag=sync status=progress

[ -n "${isloopdev}" ] && rmloopdev

exit 0
