include os.mkinc
include toolchain.mkinc

kexclusion = $(shell cat ksrc.excludes)

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
ksrc_files := $(filter-out $(kexclusion),$(ksrc_files))
ksrc_objs := $(addsuffix .o,$(ksrc_files))
ksrc_deps := $(addsuffix .d,$(ksrc_files))

kinc_opts := $(addprefix -I,$(kinc_dirs))

tmp_kbin  := $(BUILD_DIR)/tmpk.bin
ksymtable := lunaix_ksyms.o

CFLAGS += -include flags.h
CFLAGS += -include config.h

%.S.o: %.S
	$(call status_,AS,$<)
	@$(CC) $(CFLAGS) $(kinc_opts) -c $< -o $@

%.c.o: %.c
	$(call status_,CC,$<)
	@$(CC) $(CFLAGS) $(kinc_opts) -c $< -o $@

$(tmp_kbin): $(ksrc_objs)
	$(call status_,LD,$@)
	@$(CC) -T link/linker.ld $(LDFLAGS) -o $@ $^

$(ksymtable): $(tmp_kbin)
	$(call status_,KSYM,$@)
	@scripts/gen_ksymtable.sh DdRrTtAGg $< > .lunaix_ksymtable.S
	@$(CC) $(CFLAGS) -c .lunaix_ksymtable.S -o $@

.PHONY: __do_relink
__do_relink: $(ksrc_objs) $(ksymtable)
	$(call status_,LD,$(kbin))
	@$(CC) -T link/linker.ld $(LDFLAGS) -o $(kbin) $^
	@rm $(tmp_kbin)

.PHONY: all
all: __do_relink

clean:
	@rm -f $(ksrc_objs)
	@rm -f $(ksrc_deps)
	@rm -f .lunaix_ksymtable.S $(ksymtable)