# Copyright (c) 2024-2026 Yoonkyu Lee
# SPDX-License-Identifier: MIT
#
# Makefile for riscv-skyline.
#
# Layout:
#   kernel/    this Makefile + drivers + mp1.S + linker script
#   tests/     pytest-style test bench (was test/ in the upstream tree)
#   tools/     vendor demo.o / memory.o staging (gitignored, replaced by
#              kernel/demo.c + kernel/memory.c in Phase 4-5)

TOOLPREFIX = riscv64-unknown-elf-
QEMU       = qemu-system-riscv64

CC      = $(TOOLPREFIX)gcc
AS      = $(TOOLPREFIX)as
LD      = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

# demo.o is the original starter's drawing-loop driver, kept as a
# checked-in vendor binary under kernel/vendor/. It calls the eight
# primitives in mp1.S and the malloc/free in memory.c.
# memory.o is built from kernel/memory.c (clean-room allocator).
DEMO_OBJ ?= vendor/demo.o
MEM_OBJ  ?= memory.o

CORE_OBJS = \
	start.o \
	halt.o \
	string.o \
	trap.o \
	intr.o \
	serial.o \
	console.o \
	vga.o \
	mp1.o

TEST_OBJS = \
	start.o \
	string.o \
	trap.o \
	intr.o \
	serial.o \
	console.o \
	vga.o \
	mp1.o \
	../tests/halt_replace.o \
	../tests/setjmp.o \
	../tests/clobber.o \
	../tests/fb.o \
	../tests/test_globals.o \
	../tests/test_stubs.o \
	../tests/test_runner.o \
	../tests/tests_star.o \
	../tests/tests_window.o \
	../tests/tests_beacon.o \
	../tests/tests_extra.o \
	../tests/test_main.o

CFLAGS  = -Wall -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64g -mabi=lp64d
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -I. -I../tests -DDEBUG -DTRACE

# Default goal: production demo binary.
.DEFAULT_GOAL := all
.PHONY: all clean run-demo run-test debug-demo debug-test

all: demo.elf

demo.elf: $(CORE_OBJS) $(DEMO_OBJ) $(MEM_OBJ)
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

test.elf: $(TEST_OBJS) $(MEM_OBJ)
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^

# Pattern rules need to match the cross-directory test bench objects.
%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf *.o ../tests/*.o *.elf

QEMUOPTS  = -machine virt -bios none -m 128M -serial mon:stdio

run-demo: demo.elf
	$(QEMU) $(QEMUOPTS) -kernel demo.elf -device bochs-display

run-test: test.elf
	$(QEMU) $(QEMUOPTS) -kernel test.elf -nographic

debug-demo: demo.elf
	$(QEMU) $(QEMUOPTS) -kernel demo.elf -device bochs-display -S -s

debug-test: test.elf
	$(QEMU) $(QEMUOPTS) -kernel test.elf -nographic -S -s
