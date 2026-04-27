# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 The Scadek OS Project contributors

TARGET := x86_64-elf
CC := $(TARGET)-gcc
LD := $(TARGET)-ld
OBJCOPY := $(TARGET)-objcopy

BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/iso_root
KERNEL := $(BUILD_DIR)/scdk.elf
ISO := $(BUILD_DIR)/scdk.iso
INITRD_ROOT := initrd
INITRD_BUILD_ROOT := $(BUILD_DIR)/initrd_root
INITRD := $(BUILD_DIR)/scdk.initrd
USER_DIR := user
USER_BUILD_DIR := $(BUILD_DIR)/user
USER_INIT_OBJ := $(USER_BUILD_DIR)/init.o
USER_INIT_ELF := $(USER_BUILD_DIR)/init.elf
USER_INIT_BIN := $(INITRD_BUILD_ROOT)/init
USER_HELLO_OBJ := $(USER_BUILD_DIR)/hello.o
USER_HELLO_ELF := $(USER_BUILD_DIR)/hello.elf
USER_HELLO_BIN := $(INITRD_BUILD_ROOT)/hello
USER_GRANT_OBJ := $(USER_BUILD_DIR)/grant.o
USER_GRANT_ELF := $(USER_BUILD_DIR)/grant.elf
USER_GRANT_BIN := $(INITRD_BUILD_ROOT)/grant-test
USER_RING_OBJ := $(USER_BUILD_DIR)/ring.o
USER_RING_ELF := $(USER_BUILD_DIR)/ring.elf
USER_RING_BIN := $(INITRD_BUILD_ROOT)/ring-test
LIMINE_DIR := .devtools/limine
VERSION_FILE := VERSION
SCDK_VERSION := $(shell sed -n '1p' $(VERSION_FILE))
CPPFLAGS := -DSCDK_VERSION=\"$(SCDK_VERSION)\"
USER_LOAD_ADDR := 0x0000000000400000

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

USER_CFLAGS := \
	-ffreestanding \
	-fno-builtin \
	-fno-pic \
	-fno-pie \
	-m64 \
	-mno-red-zone \
	-mno-mmx \
	-mno-sse \
	-mno-sse2 \
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
INITRD_FILES := \
	$(USER_INIT_BIN) \
	$(USER_HELLO_BIN) \
	$(USER_GRANT_BIN) \
	$(USER_RING_BIN) \
	$(INITRD_BUILD_ROOT)/etc/scdk.conf \
	$(INITRD_BUILD_ROOT)/hello.txt
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

$(USER_INIT_OBJ): $(USER_DIR)/init.S
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(USER_CFLAGS) -c $< -o $@

$(USER_INIT_ELF): $(USER_INIT_OBJ)
	@mkdir -p $(dir $@)
	$(LD) -nostdlib -static -Ttext=$(USER_LOAD_ADDR) -o $@ $<

$(USER_INIT_BIN): $(USER_INIT_ELF)
	@mkdir -p $(dir $@)
	$(OBJCOPY) -O binary -j .text $< $@

$(USER_HELLO_OBJ): $(USER_DIR)/hello.S
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(USER_CFLAGS) -c $< -o $@

$(USER_HELLO_ELF): $(USER_HELLO_OBJ)
	@mkdir -p $(dir $@)
	$(LD) -nostdlib -static -Ttext=$(USER_LOAD_ADDR) -o $@ $<

$(USER_HELLO_BIN): $(USER_HELLO_ELF)
	@mkdir -p $(dir $@)
	$(OBJCOPY) -O binary -j .text $< $@

$(USER_GRANT_OBJ): $(USER_DIR)/grant.S
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(USER_CFLAGS) -c $< -o $@

$(USER_GRANT_ELF): $(USER_GRANT_OBJ)
	@mkdir -p $(dir $@)
	$(LD) -nostdlib -static -Ttext=$(USER_LOAD_ADDR) -o $@ $<

$(USER_GRANT_BIN): $(USER_GRANT_ELF)
	@mkdir -p $(dir $@)
	$(OBJCOPY) -O binary -j .text $< $@

$(USER_RING_OBJ): $(USER_DIR)/ring.S
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(USER_CFLAGS) -c $< -o $@

$(USER_RING_ELF): $(USER_RING_OBJ)
	@mkdir -p $(dir $@)
	$(LD) -nostdlib -static -Ttext=$(USER_LOAD_ADDR) -o $@ $<

$(USER_RING_BIN): $(USER_RING_ELF)
	@mkdir -p $(dir $@)
	$(OBJCOPY) -O binary -j .text $< $@

$(INITRD_BUILD_ROOT)/etc/scdk.conf: $(INITRD_ROOT)/etc/scdk.conf
	@mkdir -p $(dir $@)
	cp $< $@

$(INITRD_BUILD_ROOT)/hello.txt: $(INITRD_ROOT)/hello.txt
	@mkdir -p $(dir $@)
	cp $< $@

$(INITRD): $(INITRD_FILES)
	@mkdir -p $(dir $@)
	tar --format=ustar -cf $@ -C $(INITRD_BUILD_ROOT) init hello grant-test ring-test etc/scdk.conf hello.txt

$(BUILD_DIR)/%.c.o: src/%.c $(VERSION_FILE)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.S.o: src/%.S $(VERSION_FILE)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(ISO): $(KERNEL) $(INITRD) limine.conf
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)/boot/limine $(ISO_DIR)/EFI/BOOT
	cp $(KERNEL) $(ISO_DIR)/boot/scdk.elf
	cp $(INITRD) $(ISO_DIR)/boot/scdk.initrd
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
