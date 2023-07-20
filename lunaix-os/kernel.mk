include os.mkinc
include toolchain.mkinc

define ksrc_dirs
	kernel
	hal
	debug
	libs
	arch/$(ARCH)
endef

define kinc_dirs
	includes
	includes/usr
endef


kbin_dir := $(BUILD_DIR)
kbin := $(BUILD_NAME)

ksrc_files := $(foreach f, $(ksrc_dirs), $(shell find $(f) -name "*.[cS]"))
ksrc_objs := $(addsuffix .o,$(ksrc_files))

kinc_opts := $(addprefix -I,$(kinc_dirs))


CFLAGS += -include flags.h

%.S.o: %.S
	$(call status_,AS,$<)
	@$(CC) $(kinc_opts) -c $< -o $@

%.c.o: %.c
	$(call status_,CC,$<)
	@$(CC) $(CFLAGS) $(kinc_opts) -c $< -o $@

$(kbin): $(ksrc_objs) $(kbin_dir)
	$(call status_,LD,$@)
	@$(CC) -T link/linker.ld -o $(kbin) $(ksrc_objs) $(LDFLAGS)

all: $(kbin)

clean:
	@rm -f $(ksrc_objs)