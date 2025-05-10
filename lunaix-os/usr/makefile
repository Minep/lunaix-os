include utils.mkinc
include toolchain.mkinc

include lunabuild.mkinc

include $(lbuild_mkinc)

ifndef ARCH
$(error ARCH is not set)
endif

CFLAGS += $(BUILD_CFLAGS)
LDFLAGS += $(BUILD_LDFLAGS)

task := all

sys_include := $(CURDIR)/includes
build_dir := $(CURDIR)/build
libc_name := liblunac
libc_files := $(libc_name).a
libc := $(addprefix $(build_dir)/lib/,$(libc_files))

common_param :=	CC AR INCLUDES BUILD_DIR BUILD_NAME\
				CFLAGS LDFLAGS ARCH LBUILD LIBC_INCLUDE

INCLUDES := $(sys_include)
BUILD_DIR := $(build_dir)
BUILD_NAME := $(libc_name).a

LIBC_INCLUDE := $(build_dir)/usr/includes

mkapp-list := $(addprefix app-, $(shell cat apps.list))
mkexec-list := $(addprefix $(build_dir)/bin/, $(BUILD_SRCS))

uexec_ld := $(CURDIR)/uexec.ld

# Directory structure prerequisites
$(build_dir)/bin:
	@mkdir -p $(build_dir)/bin

$(build_dir)/lib:
	@mkdir -p $(build_dir)/lib

$(LIBC_INCLUDE):
	@mkdir -p $(LIBC_INCLUDE)

# LibC
export $(common_param)
$(build_dir)/$(libc_name).a: $(build_dir)/bin \
							 $(build_dir)/lib $(LIBC_INCLUDE)
	$(call status,TASK,$(BUILD_NAME))
	@$(MAKE) $(MKFLAGS) -C libc/ $(task)

$(uexec_ld): $(uexec_ld)x
	@$(CC) $(CFLAGS) -x c -P -E $< -o $@

# Application (with standalone makefile)
export LD_SCRIPT := $(uexec_ld)
export LIBC := $(libc)
export $(common_param)
app-%: $(uexec_ld)
	$(call status,TASK,$*)
	@$(MAKE) $(MKFLAGS) -C $* $(task) BUILD_NAME="$*"

app: task := all
app: INCLUDES += $(LIBC_INCLUDE)
app: $(mkapp-list)


# Programs (single file)
exec_%.o: %.c
	$(call status,CC,$<)
	@$(CC) $(CFLAGS) $(addprefix -I,$(INCLUDES)) -c $< -o $@

$(build_dir)/bin/%: exec_%.o $(uexec_ld)
	$(call status,LD,$(@F))
	@$(CC) -T $(uexec_ld) -o $@ $< $(libc) $(LDFLAGS)

exec: task := all
exec: INCLUDES += $(LIBC_INCLUDE)
exec: $(mkexec-list)


# General recipes
clean: task := clean
clean: $(mkapp-list)
	@rm -rf $(build_dir) $(lbuild_dir) $(uexec_ld)
	@$(MAKE) $(MKFLAGS) -C libc/ $(task)

.NOTPARALLEL:
all: task := all
all: $(build_dir)/$(libc_name).a exec app