CROSS := aarch64-elf-
CC    := $(CROSS)gcc
LD    := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy

CFLAGS := -ffreestanding -fno-stack-protector -fpic -fshort-wchar \
          -mstrict-align -I src -O2 -Wall -Wextra \
          -Wno-unused-parameter -Wno-unused-variable

LDFLAGS := -nostdlib -shared -Bsymbolic -T src/linker.ld

SRCDIR := src
BUILDDIR := build

SRCS := $(SRCDIR)/main.c $(SRCDIR)/framebuffer.c $(SRCDIR)/font.c \
        $(SRCDIR)/input.c $(SRCDIR)/desktop.c $(SRCDIR)/snake.c \
        $(SRCDIR)/pong.c $(SRCDIR)/fs.c $(SRCDIR)/notepad.c
OBJS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS)) \
        $(BUILDDIR)/crt0.o

.PHONY: all clean image run

all: $(BUILDDIR)/BOOTAA64.EFI

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/crt0.o: $(SRCDIR)/crt0.S | $(BUILDDIR)
	$(CC) -ffreestanding -c $< -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/kernel.so: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILDDIR)/BOOTAA64.EFI: $(BUILDDIR)/kernel.so
	$(OBJCOPY) -O pei-aarch64-little \
	           --subsystem efi-app \
	           --section-alignment 0x1000 \
	           --file-alignment 0x200 \
	           --image-base 0x0 \
	           $< $@
	python3 tools/fix_pe.py

image: $(BUILDDIR)/BOOTAA64.EFI
	python3 tools/mkimg.py

clean:
	rm -rf $(BUILDDIR)

run: image
	@echo "Import disk.img into UTM as an ARM64 UEFI VM and boot."
	@echo "See README.md for detailed instructions."
