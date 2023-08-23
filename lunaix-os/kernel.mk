include os.mkinc
include toolchain.mkinc

define ksrc_dirs
	kernel
	hal
	libs
	arch/$(ARCH)
endef

define kinc_dirs
	includes
	includes/usr
	arch/$(ARCH)/includes
endef


kbin_dir := $(BUILD_DIR)
kbin := $(BUILD_NAME)

ksrc_files := $(foreach f, $(ksrc_dirs), $(shell find $(f) -name "*.[cS]"))
ksrc_objs := $(addsuffix .o,$(ksrc_files))
ksrc_deps := $(addsuffix .d,$(ksrc_files))

kinc_opts := $(addprefix -I,$(kinc_dirs))


CFLAGS += -include flags.h

%.S.o: %.S
	$(call status_,AS,$<)
	@$(CC) $(kinc_opts) -c $< -o $@

%.c.o: %.c
	$(call status_,CC,$<)
	@$(CC) $(CFLAGS) $(kinc_opts) -c $< -o $@

$(kbin_dir)/modksyms: $(kbin)
	$(call status_,GEN,$@)
	@$(PY) scripts/syms_export.py --bits=32 --order=little -o "$@"  "$<" 

.PHONY: __do_relink
__do_relink: $(ksrc_objs)
	$(call status_,LD,$@)
	@$(CC) -T link/linker.ld -o $(kbin) $(ksrc_objs) $(LDFLAGS)

.PHONY: all
all: __do_relink $(kbin_dir)/modksyms

clean:
	@rm -f $(ksrc_objs)
	@rm -f $(ksrc_deps)