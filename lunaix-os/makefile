include config/make-locations
include config/make-os
include config/make-cc
include config/make-debug-tool

INCLUDES := $(patsubst %, -I%, $(INCLUDES_DIR))
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
	@echo "Compiling: $< -> $@"
	@$(CC) $(INCLUDES) -c $< -o $@

$(OBJECT_DIR)/%.c.o: %.c 
	@mkdir -p $(@D)
	@echo "Compiling: $< -> $@"
	@$(CC) $(INCLUDES) -c $< -o $@ $(CFLAGS)

$(BIN_DIR)/$(OS_BIN): $(OBJECT_DIR) $(BIN_DIR) $(SRC)
	@echo "Linking ..."
	@$(CC) -T linker.ld -o $(BIN_DIR)/$(OS_BIN) $(SRC) $(LDFLAGS)

$(BUILD_DIR)/$(OS_ISO): $(ISO_DIR) $(BIN_DIR)/$(OS_BIN) GRUB_TEMPLATE
	@./config-grub.sh ${OS_NAME} > $(ISO_GRUB_DIR)/grub.cfg
	@cp $(BIN_DIR)/$(OS_BIN) $(ISO_BOOT_DIR)
	@grub-mkrescue -o $(BUILD_DIR)/$(OS_ISO) $(ISO_DIR)

all: clean $(BUILD_DIR)/$(OS_ISO)

all-debug: O := -Og
all-debug: CFLAGS := -g -std=gnu99 -ffreestanding $(O) $(W) $(ARCH_OPT)
all-debug: LDFLAGS := -g -ffreestanding $(O) -nostdlib -lgcc
all-debug: clean $(BUILD_DIR)/$(OS_ISO)
	@echo "Dumping the disassembled kernel code to $(BUILD_DIR)/kdump.txt"
	@i686-elf-objdump -D $(BIN_DIR)/$(OS_BIN) > $(BUILD_DIR)/kdump.txt

clean:
	@rm -rf $(BUILD_DIR)

run: $(BUILD_DIR)/$(OS_ISO)
	@qemu-system-i386 -cdrom $(BUILD_DIR)/$(OS_ISO) -monitor telnet::$(QEMU_MON_PORT),server,nowait &
	@sleep 1
	@telnet 127.0.0.1 $(QEMU_MON_PORT)

debug-qemu: all-debug
	@i686-elf-objcopy --only-keep-debug $(BIN_DIR)/$(OS_BIN) $(BUILD_DIR)/kernel.dbg
	@qemu-system-i386 -s -S -cdrom $(BUILD_DIR)/$(OS_ISO) -monitor telnet::$(QEMU_MON_PORT),server,nowait &
	@sleep 1
	@$(QEMU_MON_TERM) -e "telnet 127.0.0.1 $(QEMU_MON_PORT)"
	@gdb -s $(BUILD_DIR)/kernel.dbg -ex "target remote localhost:1234"

debug-bochs: all-debug
	@bochs -q -f bochs.cfg
