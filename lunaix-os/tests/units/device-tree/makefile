obj-dut := dut/dt_interrupt.o \
			dut/dt.o \
			dut/dtspec.o \
			dut/changeling.o

BIN_DEPS += load.%.o
CFLAGS   += -DCONFIG_USE_DEVICETREE -Wp,-w

.PRECIOUS: %.dtb
%.dtb: %.dts
	$(call status,DTC,$^)
	@dtc -q -I dts -O dtb $^ -o $@

load.%.o: load.c samples/%.dtb
	$(call status,CC,$@)
	@$(CC) $(CFLAGS) -DTEST_DTBFILE=\"samples/$*.dtb\" -c $< -o $@

include units_build.mkinc