include kbuild_deps.mkinc

kbin_dir := $(BUILD_DIR)
kbin := $(BUILD_NAME)

tmp_kbin  := $(BUILD_DIR)/tmpk.bin
klinking  := link/lunaix.ld

CFLAGS += $(kcflags) -MMD -MP

all_linkable = $(filter-out $(klinking),$(1))

%.S.o: %.S $(khdr_files) kernel.mk
	$(call status,AS,$<)
	@$(CC) $(CFLAGS) -c $< -o $@

%.c.o: %.c kernel.mk
	$(call status,CC,$<)
	@$(CC) $(CFLAGS) -c $< -o $@


$(klinking): link/lunaix.ldx
	$(call status,PP,$<)
	@$(CC) $(CFLAGS) -x c -E -P $< -o $@


$(tmp_kbin): $(klinking) $(ksrc_objs)
	$(call status,LD,$@)

	@$(CC) -T $(klinking) $(config_h) $(LDFLAGS) -o $@ \
			$(call all_linkable,$^)

ksymtable := lunaix_ksyms.o
ksecsmap  := lunaix_ksecsmap.o

kautogen  := $(ksecsmap) $(ksymtable)

$(ksymtable): $(tmp_kbin)
	$(call status,KSYM,$@)
	@ARCH=$(ARCH) scripts/gen-ksymtable DdRrTtAGg $< > lunaix_ksymtable.S

	@$(CC) $(CFLAGS) -c lunaix_ksymtable.S -o $@

$(ksecsmap): $(tmp_kbin)
	$(call status,KGEN,$@)
	@scripts/elftool.tool -p -i $< > lunaix_ksecsmap.S

	@$(CC) $(CFLAGS) -c lunaix_ksecsmap.S -o $@

.PHONY: __do_relink

__do_relink: $(klinking) $(ksrc_objs) $(kautogen)
	$(call status,LD,$(kbin))

	@$(CC) -T $(klinking) $(config_h) $(LDFLAGS) -o $(kbin) \
			$(call all_linkable,$^)
	
	@rm $(tmp_kbin)


.PHONY: all
all: __do_relink


clean:
	@rm -f $(ksrc_objs)
	@rm -f $(ksrc_deps)
	@rm -f $(klinking)
	@rm -f lunaix_ksymtable.S $(ksymtable)