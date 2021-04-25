# SPDX-License-Identifier: GPL-2.0-only
# (c) William Fonkou Tambe

ifeq ($(origin ARCH), undefined)
ARCH := pu32
endif

ifeq ($(origin PREFIX), undefined)
PREFIX := /opt/pu32-toolchain
endif

# Override example: make ARCH=pu64 PREFIX=/opt/pu64-toolchain

CC := ${PREFIX}/bin/${ARCH}-elf-gcc
OBJCOPY := ${PREFIX}/bin/${ARCH}-elf-objcopy

CFLAGS := -Werror -fdata-sections -ffunction-sections -Wl,--gc-sections -Os -g3
CFLAGS += -fstack-usage

NESYS4DDR_LOADER_PART0_ELF := 0.nexys4ddr.socloader.elf
NESYS4DDR_LOADER_PART0_BIN := 0.nexys4ddr.socloader.bin
NESYS4DDR_LOADER_PART1_ELF := 1.nexys4ddr.socloader.elf
NESYS4DDR_LOADER_PART1_BIN := 1.nexys4ddr.socloader.bin

LOADER_ELF := socloader.elf
LOADER_BIN := socloader.bin

BIOS_ELF := socbios.elf
BIOS_BIN := socbios.bin

.PHONY: artifacts install clean

artifacts: ${NESYS4DDR_LOADER_PART0_BIN} ${NESYS4DDR_LOADER_PART1_BIN} \
		${LOADER_BIN} ${BIOS_BIN} mksocimg.tmp
	@echo ==== Stack Usage ====
	@cat *.su
	@echo =====================

${NESYS4DDR_LOADER_PART0_BIN}: nexys4ddr.loader.h loader.lds nexys4ddr.loader.c
	${CC} -nostdlib -I ../ ${CFLAGS} -o ${NESYS4DDR_LOADER_PART0_ELF} \
		-DNESYS4DDR_LOADER_PART0 \
		-include nexys4ddr.loader.h nexys4ddr.loader.c \
		-Wl,-Tloader.lds
	${OBJCOPY} -O binary \
		--set-section-flags .bss=alloc,load,contents \
		${NESYS4DDR_LOADER_PART0_ELF} ${NESYS4DDR_LOADER_PART0_BIN}

${NESYS4DDR_LOADER_PART1_BIN}: nexys4ddr.loader.h loader.lds nexys4ddr.loader.c
	${CC} -nostdlib -I ../ ${CFLAGS} -o ${NESYS4DDR_LOADER_PART1_ELF} \
		-DNESYS4DDR_LOADER_PART1 \
		-include nexys4ddr.loader.h nexys4ddr.loader.c \
		-Wl,-Tloader.lds
	${OBJCOPY} -O binary \
		--set-section-flags .bss=alloc,load,contents \
		${NESYS4DDR_LOADER_PART1_ELF} ${NESYS4DDR_LOADER_PART1_BIN}

${LOADER_BIN}: loader.h loader.lds loader.c
	${CC} -nostdlib -I ../ ${CFLAGS} -o ${LOADER_ELF} \
		-include loader.h loader.c \
		-Wl,-Tloader.lds
	${OBJCOPY} -O binary \
		--set-section-flags .bss=alloc,load,contents \
		${LOADER_ELF} ${LOADER_BIN}

${BIOS_BIN}: bios.h bios.lds bios.c \
             ../hwdrvchar/hwdrvchar.h ../mutex/mutex.h
	echo \#define BIOSVERSION \"bios $$(var=$$(git log -n1 --pretty=format:'%H'); echo $${var:0:8})\\n\" > version.h
	${CC} -nostdlib -I ../ ${CFLAGS} -o ${BIOS_ELF} \
		-include bios.h bios.c \
		-lgcc -Wl,-Tbios.lds
	${OBJCOPY} -O binary \
		--set-section-flags .bss=alloc,load,contents \
		${BIOS_ELF} ${BIOS_BIN}
	rm -rf version.h

mksocimg.tmp: mkimg.bash
	cp mkimg.bash mksocimg.tmp
	# https://stackoverflow.com/questions/407523/escape-a-string-for-a-sed-replace-pattern
	sed -i "s/#TO_FIX_AT_INSTALL#prefix=\"\"/prefix=\"$$(printf '%s\n' "${PREFIX}" | sed -e 's/[\/&]/\\&/g')\"/g" mksocimg.tmp

install: artifacts
	mkdir -p ${PREFIX}/{bin,lib}/
	for f in \
		${NESYS4DDR_LOADER_PART0_ELF} ${NESYS4DDR_LOADER_PART0_BIN} \
		${NESYS4DDR_LOADER_PART1_ELF} ${NESYS4DDR_LOADER_PART1_BIN} \
		${LOADER_ELF} ${LOADER_BIN} \
		${BIOS_ELF} ${BIOS_BIN} ; do \
		cp $${f} ${PREFIX}/lib/$${f}; \
	done
	cp mksocimg.tmp ${PREFIX}/bin/${ARCH}-mksocimg
	chmod +x ${PREFIX}/bin/${ARCH}-mksocimg

clean:
	rm -rf *.o *.su \
		${NESYS4DDR_LOADER_PART0_ELF} ${NESYS4DDR_LOADER_PART0_BIN} \
		${NESYS4DDR_LOADER_PART1_ELF} ${NESYS4DDR_LOADER_PART1_BIN} \
		${LOADER_ELF} ${LOADER_BIN} \
		${BIOS_ELF} ${BIOS_BIN} \
		mksocimg.tmp
