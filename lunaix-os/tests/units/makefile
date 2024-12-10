LUNAIX_ROOT ?= $(shell realpath ../../)

include $(LUNAIX_ROOT)/tests/shared/makefile
include $(LUNAIX_ROOT)/tests/shared/mkobj.mkinc

MAKEFLAGS += --no-print-directory
CFLAGS += -isystem $(unit-test-root)/stubs/includes

__test-dir := device-tree btrie
test-dir := $(addprefix test-,$(__test-dir))

obj-stubs := 

obj-tmp := 
include stubs/makefile
obj-stubs += $(addprefix $(unit-test-root)/stubs/,$(obj-tmp))

BIN_DEPS := $(obj-stubs) $(obj-shared)

export BIN_DEPS CFLAGS LUNAIX_ROOT
test-%:
	$(call status,MK,$*)
	@$(MAKE) $(MKFLAGS) -C $* $(_ACT) -I $(CURDIR)

.PHONY: all clean

all: _ACT := all
all: $(obj-stubs) $(test-dir)

run: _ACT := run
run: $(obj-stubs) $(test-dir)

clean: _ACT := clean
clean: $(test-dir)
	@rm -f $(obj-stubs) $(obj-shared)