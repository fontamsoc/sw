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

.PHONY: clean

memtest.bin: memtest.h memtest.c
	${CC} -nostdlib -I ../ ${CFLAGS} -o memtest.elf \
		-include memtest.h memtest.c \
		-Wl,-Tmemtest.lds
	${OBJCOPY} -O binary \
		--set-section-flags .bss=alloc,load,contents \
		memtest.elf memtest.bin
	hexdump -v -e '/4 "%08x "' memtest.bin > memtest.hex

clean:
	rm -rf *.su *.elf *.bin
