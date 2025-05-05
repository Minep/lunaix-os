mkinc_dir := $(CURDIR)/makeinc

include $(mkinc_dir)/toolchain.mkinc
include $(mkinc_dir)/utils.mkinc
include $(mkinc_dir)/lunabuild.mkinc

QEMU_HMP ?= 45454
ARCH ?= i386
MODE ?= debug
export ARCH

DEPS := $(CC) $(LD) $(AR)

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
	@mkdir -p $(os_img_dir)/usr

.PHONY: kernel
.PHONY: rootfs all clean-user clean tool

export BUILD_DIR=$(kbin_dir)
export BUILD_NAME=$(kbin)
kernel: $(lbuild_mkinc)
	$(call status,TASK,$(notdir $@))

	@$(MAKE) $(MKFLAGS) -I $(mkinc_dir) -f kernel.mk all

.NOTPARALLEL:
export LBUILD ARCH MODE
all: $(kbuild_dir) tool kernel

# --- begin stock user --- #

export PREFIX=usr/includes
usr/includes: install-header

export LBUILD ARCH MODE
user: usr/includes
	$(call status,TASK,$@)
	@$(MAKE) $(MKFLAGS) -C usr all -I $(mkinc_dir)

clean-user:
	@$(MAKE) -C usr clean -I $(mkinc_dir)

rootfs: usr/build
	$(call status,TASK,$(notdir $@))
	@./scripts/mkrootfs

usr/build: user

# --- end stock user --- #


clean:
	@$(MAKE) $(MKFLAGS) -C usr clean -I $(mkinc_dir)
	@$(MAKE) $(MKFLAGS) -C scripts clean -I $(mkinc_dir)
	@$(MAKE) $(MKFLAGS) -C tests/units clean
	@$(MAKE) -f kernel.mk clean -I $(mkinc_dir)
	
	@rm -rf $(kbuild_dir) || exit 1
	@rm -rf .builder || exit 1


# --- begin misc --- #

export CFLAGS=-include $(lbuild_config_h)
unit-test: $(lbuild_config_h)
	@$(MAKE) $(MKFLAGS) -C tests/units run 

tool:
	$(call status,TASK,$@)
	@$(MAKE) $(MKFLAGS) -C scripts all -I $(mkinc_dir)

install-header:
	@$(MAKE) $(MKFLAGS) -I $(mkinc_dir) -f install_headers.mk all

# --- end misc --- #