#!/usr/bin/env bash

hmp_port=45454
gdb_port=1234
default_cmd="console=/dev/ttyS0"

make CMDLINE=${default_cmd} ARCH=${ARCH} MODE=${MODE:-debug} image -j5 || exit -1

./scripts/qemu.py \
    scripts/qemus/qemu_x86_dev.json \
    --qemu-dir "${QEMU_DIR}" \
    -v KIMG=build/lunaix.iso \
    -v QMPORT=${hmp_port} \
    -v GDB_PORT=${gdb_port} \
    -v EXT2_TEST_DISC=machine/test_part.ext2 \
    -v ARCH=${ARCH} &

QMPORT=${hmp_port} gdb build/bin/kernel.bin -ex "target remote localhost:${gdb_port}"