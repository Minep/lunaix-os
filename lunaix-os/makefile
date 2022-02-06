OS_ARCH := x86

BUILD_DIR := build
KERNEL_DIR := kernel
OBJECT_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
ISO_DIR := $(BUILD_DIR)/iso
ISO_BOOT_DIR := $(ISO_DIR)/boot
ISO_GRUB_DIR := $(ISO_BOOT_DIR)/grub

INCLUDES_DIR := includes
INCLUDES := $(patsubst %, -I%, $(INCLUDES_DIR))

OS_NAME = lunaix
OS_BIN = $(OS_NAME).bin
OS_ISO = $(OS_NAME).iso

CC := i686-elf-gcc
AS := i686-elf-as

O := -O3
W := -Wall -Wextra
CFLAGS := -std=gnu99 -ffreestanding $(O) $(W)
LDFLAGS := -ffreestanding $(O) -nostdlib -lgcc

SOURCE_FILES := $(shell find -name "*.[cS]")
SRC := $(patsubst ./%, $(OBJECT_DIR)/%.o, $(SOURCE_FILES))

$(OBJECT_DIR):
	@mkdir -p $(OBJECT_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(ISO_DIR):
	@mkdir -p $(ISO_DIR)
	@mkdir -p $(ISO_BOOT_DIR)
	@mkdir -p $(ISO_GRUB_DIR)

$(OBJECT_DIR)/%.S.o: %.S
	@mkdir -p $(@D)
	$(CC) -c $< -o $@

$(OBJECT_DIR)/%.c.o: %.c 
	@mkdir -p $(@D)
	$(CC) $(INCLUDES) -c $< -o $@ $(CFLAGS)

$(BIN_DIR)/$(OS_BIN): $(OBJECT_DIR) $(BIN_DIR) $(SRC)
	$(CC) -T linker.ld -o $(BIN_DIR)/$(OS_BIN) $(SRC) $(LDFLAGS)

$(BUILD_DIR)/$(OS_ISO): $(ISO_DIR) $(BIN_DIR)/$(OS_BIN) GRUB_TEMPLATE
	@./config-grub.sh ${OS_NAME} > $(ISO_GRUB_DIR)/grub.cfg
	@cp $(BIN_DIR)/$(OS_BIN) $(ISO_BOOT_DIR)
	@grub-mkrescue -o $(BUILD_DIR)/$(OS_ISO) $(ISO_DIR)

all: clean $(BUILD_DIR)/$(OS_ISO)

all-debug: O := -O0
all-debug: CFLAGS := -g -std=gnu99 -ffreestanding $(O) $(W) -fomit-frame-pointer
all-debug: LDFLAGS := -ffreestanding $(O) -nostdlib -lgcc
all-debug: clean $(BUILD_DIR)/$(OS_ISO)
	@i686-elf-objdump -D $(BIN_DIR)/$(OS_BIN) > dump

clean:
	@rm -rf $(BUILD_DIR)

run: $(BUILD_DIR)/$(OS_ISO)
	@qemu-system-i386 -cdrom $(BUILD_DIR)/$(OS_ISO)

debug-qemu: all-debug
	@objcopy --only-keep-debug $(BIN_DIR)/$(OS_BIN) $(BUILD_DIR)/kernel.dbg
	@qemu-system-i386 -s -S -kernel $(BIN_DIR)/$(OS_BIN) &
	@gdb -s $(BUILD_DIR)/kernel.dbg -ex "target remote localhost:1234"

debug-bochs: all-debug
	@bochs -q -f bochs.cfg