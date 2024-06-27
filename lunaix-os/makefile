mkinc_dir := $(CURDIR)/makeinc

include $(mkinc_dir)/os.mkinc
include $(mkinc_dir)/toolchain.mkinc
include $(mkinc_dir)/qemu.mkinc
include $(mkinc_dir)/utils.mkinc

ARCH ?= i386
export ARCH

DEPS := $(CC) $(LD) $(AR) xorriso grub-mkrescue

CMDLINE ?= console=/dev/ttyFB0

kbuild_dir := build
kbin_dir := $(kbuild_dir)/bin
os_img_dir := $(kbuild_dir)/img

os_build_tag := $(OS_NAME)_$(ARCH)_$(OS_VER)
kbin := $(kbin_dir)/kernel.bin
kimg := $(kbuild_dir)/$(os_build_tag).iso

$(DEPS):
	@echo -n "checking $@ .... "
	@if which $@ > /dev/null; then \
		echo "ok";\
	else\
		echo "failed" && exit 1;\
	fi

define builder_data
	.builder/sources.list 
	.builder/headers.list 
	.builder/includes.list
endef

all_lconfigs = $(shell find . -name "LConfig")

$(kbuild_dir):
	@mkdir -p $(kbin_dir)
	@mkdir -p $(os_img_dir)
	@mkdir -p $(os_img_dir)/boot
	@mkdir -p $(os_img_dir)/boot/grub
	@mkdir -p $(os_img_dir)/usr

.builder/configs.h: $(all_lconfigs)
	@echo restarting configuration...
	@echo 
	@./scripts/build-tools/luna_build.py --config --lconfig-file LConfig -o $(@D)

.builder/%.list: .builder/configs.h
	@./scripts/build-tools/luna_build.py LBuild --lconfig-file LConfig -o $(@D)

.PHONY: kernel
export BUILD_DIR=$(kbin_dir)
export BUILD_NAME=$(kbin)
kernel: $(builder_data)
	$(call status,TASK,$(notdir $@))
	@$(MAKE) $(MKFLAGS) -I $(mkinc_dir) -f kernel.mk all

.PHONY: image
export KCMD=$(CMDLINE)
export _OS_NAME=$(OS_NAME)
image: usr/build kernel
	$(call status,TASK,$(notdir $@))
	@./scripts/grub/config-grub.sh $(os_img_dir)/boot/grub/grub.cfg
	@cp -r usr/build/* $(os_img_dir)/usr
	@cp -r $(kbin_dir)/* $(os_img_dir)/boot
	@grub-mkrescue -o $(kimg) $(os_img_dir)\
		-- -volid "$(OS_ID) $(OS_VER)" -system_id "$(OS_NAME)"

usr/build: user

check: $(DEPS) check-cc scripts/grub/GRUB_TEMPLATE

prepare: check $(os_img_dir)

export BUILD_MODE=release
bootable: $(kbuild_dir) image

export BUILD_MODE=debug
bootable-debug: $(kbuild_dir) image

user:
	$(call status,$@)
	@$(MAKE) $(MKFLAGS) -C usr all -I $(mkinc_dir)

all: bootable

instable: CFLAGS := -g -std=gnu99 -ffreestanding $(O) $(W) $(ARCH_OPT) -D__LUNAIXOS_DEBUG__
instable: all

all-debug: bootable-debug

clean:
	@$(MAKE) -C usr clean -I $(mkinc_dir)
	@$(MAKE) -f kernel.mk clean -I $(mkinc_dir)
	@rm -rf $(kbuild_dir) || exit 1
	@rm -f .builder/*.list || exit 1

run: all
	@qemu-system-i386 $(call get_qemu_options,$(kimg))
	@sleep 1
	@telnet 127.0.0.1 $(QEMU_MON_PORT)

debug-qemu: all-debug
	@objcopy --only-keep-debug $(kbin) $(kbuild_dir)/kernel.dbg
	@qemu-system-i386 $(call get_qemu_options,$(kimg))
	@sleep 1
	@QMPORT=$(QEMU_MON_PORT) gdb $(kbin) -ex "target remote localhost:1234"

debug-qemu-vscode: all-debug
	@qemu-system-i386 $(call get_qemu_options,$(kimg))
	@sleep 0.5
	@telnet 127.0.0.1 $(QEMU_MON_PORT)

debug-bochs: all-debug
	@bochs -q -f bochs.cfg

debug-metal: 
	@printf "@cmc" > $(PORT)
	@gdb -s $(kbuild_dir)/kernel.dbg -ex "target remote $(PORT)"