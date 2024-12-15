include $(LUNAIX_ROOT)/makeinc/utils.mkinc

lunaix-root := $(LUNAIX_ROOT)
unit-test-root := $(lunaix-root)/tests/units
test-root := $(lunaix-root)/tests
test-shared-root := $(test-root)/shared

CFLAGS += -idirafter $(lunaix-root)/includes	\
		  -I $(test-root)/includes \
		  -Wno-discarded-qualifiers \
		  -Wno-scalar-storage-order \
		  -Werror \
		  -g \
		  -D__off_t_defined

obj-shared := $(test-shared-root)/framework.o	\
				$(test-shared-root)/memchk.o
