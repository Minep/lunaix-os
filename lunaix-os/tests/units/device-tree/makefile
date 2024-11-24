include test_build.mkinc

obj = dut/dt_interrupt.o \
	  dut/dt.o \
	  dut/changeling.o \

dtbs := samples/inheritence.dtb	\
		samples/basic.dtb

tests := basic inheritence
tests := $(addsuffix .test,$(tests))
run_tests := $(addprefix run.,$(tests))

%.dtb: %.dts
	$(call status,DTC,$^)
	@dtc -q -I dts -O dtb $^ -o $@

.PHONY: all run clean

load.%.o:: load.c
	$(call status,CC,$@)
	@$(CC) $(CFLAGS) -DTEST_DTBFILE=\"samples/$*.dtb\" -c $^ -o $@

%.test: $(obj-shared) $(obj-stubs) $(obj) test-%.o load.%.o
	@$(CC) $^ -o $@

run.%.test: %.test
	$(call status,RUN,$^)
	@./$^

all: $(dtbs) $(tests)

run: $(dtbs) $(tests) $(run_tests)

clean:
	@rm -f *.o test samples/*.dtb