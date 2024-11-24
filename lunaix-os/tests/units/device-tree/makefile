include test_build.mkinc

obj = dut/dt_interrupt.o \
	  dut/dt.o \
	  dut/changeling.o \
	  main.o

test: $(obj-stubs) $(obj)
	@$(CC) $^ -o $@

all: test

clean:
	@rm -f $(obj) test