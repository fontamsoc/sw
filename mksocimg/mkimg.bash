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
usage: $(basename ${0}) [options] <image>

	Utility that creates a standard BIOS partition table on <image>
	writting it with boot-code bios-code kernel-code and/or rootfs-data.
	It also creates a FAT32 filesystem in the first partition of <image>.

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
__EOF__
}

imgfile=""

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
		-*)
			echo error: invalid option: $1
			usage
			exit 1
			;;
		*)
			if [ -z "${imgfile}" ]; then
				imgfile="$1"
			else
				echo error: invalid extra argument
				usage
				exit 1
			fi
	esac
	shift
done

[ -z "${imgfile}" ] && {
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

set -x

tmpdir="$(mktemp -d)"
[ -z "${tmpdir}" ] && {
	echo error: mktemp failed
	exit 1
}
mkdir -p ${tmpdir}/tmp

trap 'rm -rf "${tmpdir}"' EXIT

cat > ${tmpdir}/genimage.cfg << __EOF__
image fatpart.img {
  vfat {}
  empty = true
  temporary = true
  size = 32M
}

image $(basename ${imgfile}) {
  hdimage {
    partition-table-type = "mbr"
  }

  partition socloader {
    in-partition-table = false
    offset = 0
    image = "$(realpath ${opt_loader})"
    holes = {"(440; 512)"}
  }

  partition fatpart {
    partition-type = 0xc
    image = "fatpart.img"
  }

  partition socbios {
    partition-type = 0xf8
    image = "$(realpath ${opt_bios})"
  }

  partition kernel {
    partition-type = 0x8a
    image = "$(realpath ${opt_kernel})"
  }

  partition rootfs {
    partition-type = 0x83
    image = "$(realpath ${opt_rootfs})"
  }
}
__EOF__

genimage --loglevel 9 --tmppath "${tmpdir}/tmp" --outputpath "$(dirname ${imgfile})" --config "${tmpdir}/genimage.cfg" || {
	echo error: genimage failed
	exit 1
}

exit 0
