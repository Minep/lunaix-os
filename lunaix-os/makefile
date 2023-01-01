include config/make-locations
include config/make-os
include config/make-cc
include config/make-debug-tool

DEPS := $(CC) $(LD) $(AR) xorriso grub-mkrescue

$(DEPS):
	@echo -n "checking $@ .... "
	@if which $@ > /dev/null; then \
		echo "ok";\
	else\
		echo "failed" && exit 1;\
	fi

check-cc:
	@echo -n "checking target i686-elf.... "
	@test "`i686-elf-gcc -dumpmachine`" = "i686-elf" && echo ok || (echo "failed" && exit 1)

$(OBJECT_DIR):
	@mkdir -p $(OBJECT_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(USR_DIR):
	@mkdir -p $(USR_DIR)

$(ISO_DIR):
	@mkdir -p $(ISO_DIR)
	@mkdir -p $(ISO_BOOT_DIR)
	@mkdir -p $(ISO_GRUB_DIR)

check: $(DEPS) check-cc GRUB_TEMPLATE

prepare: check $(OBJECT_DIR) $(BIN_DIR) $(ISO_DIR) $(USR_DIR)

usrlib: prepare makefile.usr
	@make -f makefile.usr usr-runtime

usrlib-debug: prepare makefile.usr
	@make -f makefile.usr usr-runtime-debug

usr-prog: prepare usrlib makefile.prog
	@make -f makefile.prog all

bootable: usr-prog usrlib makefile.ker
	@make -f makefile.ker all

bootable-debug: usr-prog usrlib-debug makefile.ker
	@make -f makefile.ker all-debug

all: bootable

instable: CFLAGS := -g -std=gnu99 -ffreestanding $(O) $(W) $(ARCH_OPT) -D__LUNAIXOS_DEBUG__
instable: all

all-debug: clean bootable-debug
	@echo "Dumping the disassembled kernel code to $(BUILD_DIR)/kdump.txt"
	@i686-elf-objdump -S $(BIN_DIR)/$(OS_BIN) > $(BUILD_DIR)/kdump.txt

clean:
	@rm -rf $(BUILD_DIR) || exit 1

run: $(BUILD_DIR)/$(OS_ISO)
	@qemu-system-i386 $(QEMU_OPTIONS)
	@sleep 1
	@telnet 127.0.0.1 $(QEMU_MON_PORT)

debug-qemu: all-debug
	@i686-elf-objcopy --only-keep-debug $(BIN_DIR)/$(OS_BIN) $(BUILD_DIR)/kernel.dbg
	@qemu-system-i386 $(QEMU_OPTIONS)
	@sleep 1
	@$(QEMU_MON_TERM) -- telnet 127.0.0.1 $(QEMU_MON_PORT)
	@gdb -s $(BUILD_DIR)/kernel.dbg -ex "target remote localhost:1234"

debug-qemu-vscode: all-debug
	@i686-elf-objcopy --only-keep-debug $(BIN_DIR)/$(OS_BIN) $(BUILD_DIR)/kernel.dbg
	@qemu-system-i386 $(QEMU_OPTIONS)
	@sleep 0.5
	@telnet 127.0.0.1 $(QEMU_MON_PORT)

debug-bochs: all-debug
	@bochs -q -f bochs.cfg

debug-metal: 
	@printf "@cmc" > $(PORT)
	@gdb -s $(BUILD_DIR)/kernel.dbg -ex "target remote $(PORT)"