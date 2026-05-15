# List all of the source files that will be compiled into your binary.
#
# For example, if you have the following source files
#
#   main.c
#   user.c
#   driver.s
#
# then your SRCS list would be
#
#   SRCS=main.c user.c driver.s
#
# The list may span several lines of text by appending a backslash to each line,
# for example:
#
#   SRCS=main.c user.c \
#        driver.s
SRCS=main.c

# Specify the CPU type that you are targeting your build towards.
#
# Supported architectures can usually be found with the --target-help argument
# passed to gcc, but a quick summary is:
#
# 68000, 68010, 68020, 68030, 68040, 68060, cpu32 (includes 68332 and 68360),
# 68302
CPU=cpu32

# Uncomment either of the following depending on how you have installed gcc on
# your system. m68k-linux-gnu for Linux installations, m68k-eabi-elf if gcc was
# built from scratch e.g. on a Mac by running the build script.
# PREFIX=m68k-linux-gnu
PREFIX=m68k-eabi-elf

# Dont modify below this line (unless you know what youre doing).
BUILDDIR=build

CC=$(PREFIX)-gcc
LD=$(PREFIX)-ld
OBJCOPY=$(PREFIX)-objcopy
OBJDUMP=$(PREFIX)-objdump

# Auto-detect m68k_bare_metal path
ifndef m68kbm_PATH
ifneq ($(wildcard ../include/.*),)
    m68kbm_PATH=..
endif
ifneq ($(wildcard ../m68k_bare_metal/include/.*),)
    m68kbm_PATH=../m68k_bare_metal
endif
endif

CFLAGS=-m$(CPU) -Wall -g -static -I${m68kbm_PATH}/include -I. -msoft-float -MMD -MP -Os -fno-omit-frame-pointer -falign-functions=4 -falign-jumps=4 -falign-loops=4 -DVERSION_$(VERSION)
LFLAGS=--script=pcrel_application.ld -L${m68kbm_PATH}/libmetal -lmetal-$(CPU)

OBJS=$(patsubst %.c,$(BUILDDIR)/%.c.o,$(SRCS))
OBJS:=$(patsubst %.S,$(BUILDDIR)/%.S.o,$(OBJS))
OBJS:=$(patsubst %.s,$(BUILDDIR)/%.s.o,$(OBJS))
DEPS=$(OBJS:.o=.d)

.PHONY: loader release all crt clean rom srec dump dumps hexdump

loader: crt $(OBJS)
	$(LD) -o $@ $(OBJS) $(LFLAGS)

release: CFLAGS+= -DNDEBUG
release: all

$(BUILDDIR):
	mkdir -p $@

$(BUILDDIR)/%.c.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/%.S.o: %.S
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/%.s.o: %.s
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

-include $(DEPS)

all: crt loader rom srec

crt: crt0.S
	$(CC) $(CFLAGS) -c -o crt0.o crt0.S
	rm -f crt0.d

clean:
	rm -rf $(BUILDDIR)/*
	rm -f *.o *.d
	rm -f loader*

rom: loader
	$(OBJCOPY) -O binary --gap-fill 0xff --pad-to 0xfc0 loader loader.rom

srec: loader
	$(OBJCOPY) -O srec loader loader.srec

xc: loader
	$(OBJCOPY) -O binary loader Loader.xc

dump: loader
	$(OBJDUMP) -mm68k:$(CPU) -belf32-m68k -dt -j.text loader
	$(OBJDUMP) -mm68k:$(CPU) -belf32-m68k -st -j.data -j.bss loader

dumps: loader
	$(OBJDUMP) -mm68k:$(CPU) -belf32-m68k -St -j.text loader
	$(OBJDUMP) -mm68k:$(CPU) -belf32-m68k -st -j.data -j.bss loader

hexdump: rom
	hexdump -C loader.rom
