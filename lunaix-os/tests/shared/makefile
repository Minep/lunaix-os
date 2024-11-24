include $(LUNAIX_ROOT)/makeinc/utils.mkinc

lunaix-root := $(LUNAIX_ROOT)
unit-test-root := $(lunaix-root)/tests/units
test-root := $(lunaix-root)/tests
test-shared-root := $(test-root)/shared

CFLAGS += -idirafter $(lunaix-root)/includes	\
		  -I $(test-root)/includes \
		  -Wno-discarded-qualifiers \
		  -g


%.o: %.c
	$(call status,CC,$(@F))
	@$(CC) $(CFLAGS) -c $< -o $@


obj-shared := $(test-shared-root)/framework.o
