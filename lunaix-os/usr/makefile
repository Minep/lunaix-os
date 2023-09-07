include utils.mkinc
include toolchain.mkinc

task := all

sys_include := $(CURDIR)/includes
build_dir := $(CURDIR)/build
libc_name := liblunac
libc_files := $(libc_name).a

common_param :=	CC AR INCLUDES BUILD_DIR BUILD_NAME CFLAGS LDFLAGS

INCLUDES := $(sys_include)
BUILD_DIR := $(build_dir)
BUILD_NAME := $(libc_name).a

$(build_dir)/bin:
	@mkdir -p $(build_dir)/bin

$(build_dir)/lib:
	@mkdir -p $(build_dir)/lib

$(build_dir)/includes:
	@mkdir -p $(build_dir)/includes

export $(common_param)
$(build_dir)/$(libc_name).a: $(build_dir)/bin $(build_dir)/lib $(build_dir)/includes
	$(call status,TASK,$(BUILD_NAME))
	@$(MAKE) $(MKFLAGS) -C libc/ $(task)

app-list := ls
app-list += init
app-list += signal_demo
app-list += sh
app-list += cat
app-list += testp
app-list += stat

mkapp-list := $(addprefix app-, $(app-list))

export LD_SCRIPT := $(CURDIR)/link-usr.ld
export LIBC := $(addprefix $(build_dir)/lib/,$(libc_files))
app-%:
	$(call status,TASK,$*)
	@$(MAKE) $(MKFLAGS) -C $* $(task) BUILD_NAME="$*"

app: task := all
app: INCLUDES += $(build_dir)/includes
app: $(mkapp-list)

clean: task := clean
clean: $(mkapp-list)
	@rm -rf $(build_dir)
	@$(MAKE) $(MKFLAGS) -C libc/ $(task)

all: task := all
all: $(build_dir)/$(libc_name).a app