mkinc_dir := $(CURDIR)/makeinc

include $(mkinc_dir)/toolchain.mkinc
include $(mkinc_dir)/utils.mkinc
include $(mkinc_dir)/lunabuild.mkinc

QEMU_HMP ?= 45454
ARCH ?= i386
MODE ?= debug
export ARCH

DEPS := $(CC) $(LD) $(AR) xorriso grub-mkrescue

CMDLINE ?= console=/dev/ttyFB0

kbuild_dir := build
kbin_dir := $(kbuild_dir)/bin
os_img_dir := $(kbuild_dir)/img

kbin := $(kbin_dir)/kernel.bin
kimg := $(kbuild_dir)/lunaix.iso

$(DEPS):
	@echo -n "checking $@ .... "
	@if which $@ > /dev/null; then \
		echo "ok";\
	else\
		echo "failed" && exit 1;\
	fi

$(kbuild_dir):
	@mkdir -p $(kbin_dir)
	@mkdir -p $(os_img_dir)
	@mkdir -p $(os_img_dir)/boot
	@mkdir -p $(os_img_dir)/boot/grub
	@mkdir -p $(os_img_dir)/usr

.PHONY: kernel
export BUILD_DIR=$(kbin_dir)
export BUILD_NAME=$(kbin)
kernel: $(lbuild_mkinc)
	$(call status,TASK,$(notdir $@))

	@$(MAKE) $(MKFLAGS) -I $(mkinc_dir) -f kernel.mk all

.NOTPARALLEL:
.PHONY: image
export KCMD=$(CMDLINE)
export LBUILD ARCH MODE
image: $(kbuild_dir) kernel usr/build
	$(call status,TASK,$(notdir $@))
	$(call status,PACK,$(kimg))

	@./scripts/grub/config-grub.sh $(os_img_dir)/boot/grub/grub.cfg
	@cp -r usr/build/* $(os_img_dir)/usr
	@cp -r $(kbin_dir)/* $(os_img_dir)/boot
	@grub-mkrescue -o $(kimg) $(os_img_dir)\
		-- -volid "$(OS_ID) $(OS_VER)" -system_id "$(OS_NAME)" \
		-report_about FAILURE -abort_on FAILURE

usr/build: user

prepare: $(os_img_dir)

export LBUILD ARCH MODE
user:
	$(call status,TASK,$@)
	@$(MAKE) $(MKFLAGS) -C usr all -I $(mkinc_dir)

clean:
	@$(MAKE) -C usr clean -I $(mkinc_dir)
	@$(MAKE) -f kernel.mk clean -I $(mkinc_dir)
	@rm -rf $(kbuild_dir) || exit 1
	@rm -rf .builder || exit 1
