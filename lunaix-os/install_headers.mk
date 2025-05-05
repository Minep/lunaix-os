include kbuild_deps.mkinc

ifndef PREFIX
	$(error "Must specify PREFIX to header install location")
endif

USR_HEADER := includes/usr

HEADERS := $(shell cat $(USR_HEADER)/headers)
HEADERS += $(shell cat $(USR_HEADER)/headers_autogen)

INSTALL := $(addprefix $(PREFIX)/,$(HEADERS))

export CFLAGS=$(kcflags)
$(PREFIX)/lunaix/syscallid.h:
	@scripts/gen-syscall-header "$@"

.SECONDEXPANSION:
.PRECIOUS: $(PREFIX)/%/

$(PREFIX)/%/:
	@mkdir -p $@

$(PREFIX)/%.h : $(USR_HEADER)/%.h $$(dir $$@)
	$(call status,INSTALL,$@)
	@cp $< $@

all: $(INSTALL)