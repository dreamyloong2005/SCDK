# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 The Scadek OS Project contributors

TARGET := x86_64-elf
CC := $(TARGET)-gcc
OBJCOPY := $(TARGET)-objcopy

BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/iso_root
KERNEL := $(BUILD_DIR)/scdk.elf
ISO := $(BUILD_DIR)/scdk.iso
LIMINE_DIR := .devtools/limine
VERSION_FILE := VERSION
SCDK_VERSION := $(shell sed -n '1p' $(VERSION_FILE))
CPPFLAGS := -DSCDK_VERSION=\"$(SCDK_VERSION)\"

CFLAGS := \
	-std=gnu11 \
	-ffreestanding \
	-fno-builtin \
	-fno-stack-protector \
	-fno-pic \
	-fno-pie \
	-m64 \
	-mno-red-zone \
	-mno-mmx \
	-mno-sse \
	-mno-sse2 \
	-mcmodel=kernel \
	-I src/include \
	-Wall \
	-Wextra \
	-Werror

LDFLAGS := \
	-T linker.ld \
	-nostdlib \
	-static \
	-Wl,-z,max-page-size=0x1000 \
	-Wl,-z,noexecstack \
	-Wl,--build-id=none

C_SRCS := $(shell find src -name '*.c' | sort)
ASM_SRCS := $(shell find src -name '*.S' | sort)
OBJS := \
	$(patsubst src/%.c,$(BUILD_DIR)/%.c.o,$(C_SRCS)) \
	$(patsubst src/%.S,$(BUILD_DIR)/%.S.o,$(ASM_SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean iso run

all: $(KERNEL)

iso: $(ISO)

run: iso
	tools/run-qemu.sh

$(KERNEL): $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@ -lgcc

$(BUILD_DIR)/%.c.o: src/%.c $(VERSION_FILE)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.S.o: src/%.S $(VERSION_FILE)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(ISO): $(KERNEL) limine.conf
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)/boot/limine $(ISO_DIR)/EFI/BOOT
	cp $(KERNEL) $(ISO_DIR)/boot/scdk.elf
	cp limine.conf $(ISO_DIR)/boot/limine/limine.conf
	cp $(LIMINE_DIR)/limine-bios.sys $(ISO_DIR)/boot/limine/
	cp $(LIMINE_DIR)/limine-bios-cd.bin $(ISO_DIR)/boot/limine/
	cp $(LIMINE_DIR)/limine-uefi-cd.bin $(ISO_DIR)/boot/limine/
	cp $(LIMINE_DIR)/BOOTX64.EFI $(ISO_DIR)/EFI/BOOT/BOOTX64.EFI
	cp $(LIMINE_DIR)/BOOTIA32.EFI $(ISO_DIR)/EFI/BOOT/BOOTIA32.EFI
	xorriso -as mkisofs \
		-b boot/limine/limine-bios-cd.bin \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part \
		--efi-boot-image \
		--protective-msdos-label \
		$(ISO_DIR) \
		-o $(ISO)
	$(LIMINE_DIR)/limine bios-install $(ISO)

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
