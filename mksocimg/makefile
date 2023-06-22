# SPDX-License-Identifier: GPL-2.0-only
# (c) William Fonkou Tambe

ifeq ($(origin ARCH), undefined)
ARCH := pu32
endif

ifeq ($(origin PREFIX), undefined)
PREFIX := /opt/${ARCH}-toolchain
endif

.PHONY: all install clean

all: genimage/genimage mksocimg.tmp

genimage/genimage:
	[ ! -d genimage ] && \
		git clone https://github.com/pengutronix/genimage.git && \
		cd genimage && git checkout fcf9480cbf18bdd65cf41260f19372de7ce05116 || true
	[ ! -e genimage/configure ] && \
		cd genimage && ./autogen.sh || true
	cd genimage && ./configure --prefix=${PREFIX} && make

mksocimg.tmp: mkimg.bash
	cp mkimg.bash mksocimg.tmp
	# https://stackoverflow.com/questions/407523/escape-a-string-for-a-sed-replace-pattern
	sed -i "s/#TO_FIX_AT_INSTALL#prefix=\"\"/prefix=\"$$(printf '%s\n' "${PREFIX}" | sed -e 's/[\/&]/\\&/g')\"/g" mksocimg.tmp

install: all
	make -C genimage install
	cp mksocimg.tmp ${PREFIX}/bin/${ARCH}-mksocimg
	chmod +x ${PREFIX}/bin/${ARCH}-mksocimg

clean:
	[ -e genimage/Makefile ] && make -C genimage clean || true
	rm -rf mksocimg.tmp
