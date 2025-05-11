include lunabuild.mkinc

include $(lbuild_mkinc)

libc_include := $(CURDIR)/includes

BUILD_DIR ?= bin
BUILD_NAME ?= liblunac

src_dirs := src
src_dirs += arch/$(ARCH)

obj_files := $(addsuffix .o, $(BUILD_SRCS))

build_lib := $(BUILD_DIR)/lib

libc_include_opt = $(addprefix -I, $(libc_include))
global_include_opt = $(addprefix -I, $(INCLUDES) $(BUILD_INC))

check_folders := $(src_dirs)
check_folders += $(build_lib) $(LIBC_INCLUDE)

$(BUILD_DIR):
	@mkdir -p bin

%.check:
	@if [ ! -d "$*" ]; then \
		echo "'$*' is not exists" && exit 1;\
	fi

%.c.o: %.c
	@$(call status_,CC,$@)
	@$(CC) $(CFLAGS) $(libc_include_opt) $(global_include_opt) -c $< -o $@

%.S.o: %.S
	@$(call status_,AS,$@)
	@$(CC) $(CFLAGS) $(libc_include_opt) $(global_include_opt) -c $< -o $@

$(build_lib)/$(BUILD_NAME): $(obj_files)
	@$(call status_,AR,$(*F))
	@$(AR) rcs $@ $^

clean:
	@rm -rf $(lbuild_dir)
	@rm -f $(obj_files)

headers: $(libc_include)
	@$(call status_,INSTALL,$(<F))
	@cp -r $(libc_include)/* $(LIBC_INCLUDE)/

all: $(addsuffix .check, $(check_folders)) $(build_lib)/$(BUILD_NAME) headers
	@cp arch/$(ARCH)/crt0.S.o $(build_lib)/crt0.o