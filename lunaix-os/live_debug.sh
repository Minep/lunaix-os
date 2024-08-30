#!/usr/bin/env bash

hmp_port=45454
gdb_port=1234
default_cmd="console=/dev/ttyS0"

make CMDLINE=${default_cmd} ARCH=${ARCH} MODE=${MODE:-debug} all -j5 || exit -1

./scripts/qemu.py \
    scripts/qemus/qemu_x86_dev.json \
    --qemu-dir "${QEMU_DIR}" \
    -v QMPORT=${hmp_port} \
    -v GDB_PORT=${gdb_port} \
    -v ROOTFS=lunaix_rootfs.ext2 \
    -v ARCH=${ARCH} \
    -v KBIN=build/bin/kernel.bin \
    -v "KCMD=${default_cmd} rootfs=/dev/block/sda init=/bin/init" \
    -- \
    -nographic &

QMPORT=${hmp_port} gdb build/bin/kernel.bin -ex "target remote localhost:${gdb_port}"