# Based on xv6-riscv Makefile; see LICENSE.

TOOLPREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CC=$(TOOLPREFIX)gcc
AS=$(TOOLPREFIX)as
LD=$(TOOLPREFIX)ld
OBJCOPY=$(TOOLPREFIX)objcopy
OBJDUMP=$(TOOLPREFIX)objdump

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

# TEST_OBJS = CORE_OBJS minus halt.o (replaced by test/halt_replace.o), plus
# test bench objects.  demo.o is NOT linked; test/test_main.o provides main()
# and test/test_globals.o provides the four skyline globals.
TEST_OBJS = \
	start.o \
	string.o \
	trap.o \
	intr.o \
	serial.o \
	console.o \
	vga.o \
	mp1.o \
	test/halt_replace.o \
	test/setjmp.o \
	test/clobber.o \
	test/fb.o \
	test/test_globals.o \
	test/test_stubs.o \
	test/test_runner.o \
	test/tests_star.o \
	test/tests_window.o \
	test/tests_beacon.o \
	test/tests_extra.o \
	test/test_main.o

CFLAGS = -Wall -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64g -mabi=lp64d
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -I. -Itest -DDEBUG -DTRACE

all: demo.elf

demo.elf: $(CORE_OBJS)
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^ demo.o memory.o

test.elf: $(TEST_OBJS)
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $^ memory.o

clean:
	rm -rf console.o halt.o intr.o serial.o start.o string.o trap.o vga.o mp1.o
	rm -rf test/*.o
	rm -rf *.elf

run-demo:
	$(QEMU) -machine virt -bios none -kernel demo.elf -m 128M -serial mon:stdio -device bochs-display

run-test:
	$(QEMU) -machine virt -bios none -kernel test.elf -m 128M -serial mon:stdio -nographic
