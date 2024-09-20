include lunabuild.mkinc
include utils.mkinc

CFLAGS := -I$(lbuild_config_h)

SRC  := elftool
OUT  := $(addsuffix .tool,$(SRC))

%.tool : %.c
	$(call status,CC,$<)
	@cc $< -o $@

.PHONY: all clean

all: $(OUT)

clean:
	rm -f $(OUT)