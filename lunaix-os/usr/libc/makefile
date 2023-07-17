libc_include := $(CURDIR)/includes

ARCH ?= i386
BUILD_DIR ?= bin
BUILD_NAME ?= liblunac

src_dirs := src
src_dirs += arch/$(ARCH)

src_files := $(foreach f, $(src_dirs), $(shell find $(f) -name "*.[cS]"))
obj_files := $(addsuffix .o, $(src_files))

build_lib := $(BUILD_DIR)/lib
build_include := $(BUILD_DIR)/includes

libc_include_opt = $(addprefix -I, $(libc_include))
global_include_opt = $(addprefix -I, $(INCLUDES))

check_folders := $(src_dirs)
check_folders += $(build_lib) $(build_include)

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
	@$(call status_,CC,$@)
	@$(CC) $(libc_include_opt) $(global_include_opt) -c $< -o $@

$(build_lib)/$(BUILD_NAME): $(obj_files)
	@$(call status_,AR,$(*F))
	@$(AR) rcs $@ $^

clean:
	@rm -f $(obj_files)

headers: $(libc_include)
	@$(call status_,INSTALL,$(<F))
	@cp -r $(libc_include)/* $(build_include)/

all: $(addsuffix .check, $(check_folders)) $(build_lib)/$(BUILD_NAME) headers
	@cp arch/$(ARCH)/crt0.S.o $(build_lib)/crt0.o