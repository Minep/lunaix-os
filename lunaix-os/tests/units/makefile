LUNAIX_ROOT ?= $(shell realpath ../../)

include test_build.mkinc

__test-dir := device-tree
test-dir := $(addprefix test-,$(__test-dir))

obj-stubs := 

obj-tmp := 
include stubs/makefile
obj-stubs += $(addprefix $(unit-test-root)/stubs/,$(obj-tmp))

export obj-stubs LUNAIX_ROOT
test-%:
	$(call status,MK,$*)
	@$(MAKE) $(MKFLAGS) -C $* $(_ACT) -I $(CURDIR)

.PHONY: all clean

all: _ACT := all
all: $(obj-stubs) $(test-dir)

clean: _ACT := clean
clean: $(test-dir)
	@rm -f $(obj-stubs)