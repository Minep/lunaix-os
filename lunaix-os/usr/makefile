include utils.mkinc
include toolchain.mkinc

task := all

sys_include := $(CURDIR)/includes
build_dir := $(CURDIR)/build
libc_name := liblunac
libc_files := $(libc_name).a
libc := $(addprefix $(build_dir)/lib/,$(libc_files))
ldscript := $(CURDIR)/link-usr.ld

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

app-list = $(shell cat apps.list)
exec-list = $(shell cat execs.list)

mkapp-list := $(addprefix app-, $(app-list))
mkexec-list := $(addprefix $(build_dir)/bin/, $(exec-list))

export LD_SCRIPT := $(ldscript)
export LIBC := $(libc)
app-%:
	$(call status,TASK,$*)
	@$(MAKE) $(MKFLAGS) -C $* $(task) BUILD_NAME="$*"

exec_%.o: %.c
	$(call status,CC,$<)
	@$(CC) $(CFLAGS) $(addprefix -I,$(INCLUDES)) -c $< -o $@

$(build_dir)/bin/%: exec_%.o
	$(call status,LD,$(@F))
	@$(CC) -T $(ldscript) -o $@ $< $(libc) $(LDFLAGS)

app: task := all
app: INCLUDES += $(build_dir)/includes
app: $(mkapp-list)

exec: task := all
exec: INCLUDES += $(build_dir)/includes
exec: $(mkexec-list)

clean: task := clean
clean: $(mkapp-list)
	@rm -rf $(build_dir)
	@$(MAKE) $(MKFLAGS) -C libc/ $(task)

all: task := all
all: $(build_dir)/$(libc_name).a exec app