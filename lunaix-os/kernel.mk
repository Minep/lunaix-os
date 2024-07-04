include os.mkinc
include toolchain.mkinc

include .builder/lbuild.mkinc

kbin_dir := $(BUILD_DIR)
kbin := $(BUILD_NAME)

ksrc_objs := $(addsuffix .o,$(_LBUILD_SRCS))
ksrc_deps := $(addsuffix .d,$(_LBUILD_SRCS))
khdr_opts := $(addprefix -include ,$(_LBUILD_HDRS))
kinc_opts := $(addprefix -I,$(_LBUILD_INCS))
config_h += -include.builder/configs.h

tmp_kbin  := $(BUILD_DIR)/tmpk.bin
ksymtable := lunaix_ksyms.o
klinking  := link/lunaix.ld

CFLAGS += $(khdr_opts) $(kinc_opts) $(config_h) -MMD -MP

-include $(ksrc_deps)

all_linkable = $(filter-out $(klinking),$(1))


%.S.o: %.S $(khdr_files) kernel.mk
	$(call status_,AS,$<)
	@$(CC) $(CFLAGS) -c $< -o $@

%.c.o: %.c $(khdr_files) kernel.mk
	$(call status_,CC,$<)
	@$(CC) $(CFLAGS) -c $< -o $@


$(klinking): link/lunaix.ldx
	@$(CC) $(CFLAGS) -x c -E -P $< -o $@


$(tmp_kbin): $(ksrc_objs)
	$(call status_,LD,$@)

	@$(CC) -T $(klinking) $(config_h) $(LDFLAGS) -o $@ \
			$(call all_linkable,$^)


$(ksymtable): $(tmp_kbin)
	$(call status_,KSYM,$@)
	@scripts/gen_ksymtable.sh DdRrTtAGg $< > .lunaix_ksymtable.S

	@$(CC) $(CFLAGS) -c .lunaix_ksymtable.S -o $@


.PHONY: __do_relink
__do_relink: $(ksrc_objs) $(ksymtable)
	$(call status_,LD,$(kbin))

	@$(CC) -T $(klinking) $(config_h) $(LDFLAGS) -o $(kbin) \
			$(call all_linkable,$^)
	
	@rm $(tmp_kbin)


.PHONY: all
all: __do_relink


clean:
	@rm -f $(ksrc_objs)
	@rm -f $(ksrc_deps)
	@rm -f .lunaix_ksymtable.S $(ksymtable)