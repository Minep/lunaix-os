include os.mkinc
include toolchain.mkinc

ksrc_files = $(shell cat .builder/sources.list)
kinc_dirs  = $(shell cat .builder/includes.list)
khdr_files = $(shell cat .builder/headers.list)
khdr_files += .builder/configs.h

kbin_dir := $(BUILD_DIR)
kbin := $(BUILD_NAME)

ksrc_objs := $(addsuffix .o,$(ksrc_files))
ksrc_deps := $(addsuffix .d,$(ksrc_files))
khdr_opts := $(addprefix -include ,$(khdr_files))
kinc_opts := $(addprefix -I,$(kinc_dirs))

tmp_kbin  := $(BUILD_DIR)/tmpk.bin
ksymtable := lunaix_ksyms.o

CFLAGS += $(khdr_opts)

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