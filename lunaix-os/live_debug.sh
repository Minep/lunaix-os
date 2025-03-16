#!/usr/bin/env bash

hmp_port=45454
gdb_port=1234
default_cmd="console=/dev/ttyS0"

if [[ -z "${ARCH}" ]]; then
    echo "error: please specify ARCH="
    exit 1
fi

make ARCH="${ARCH}" MODE="${MODE:-debug}" all -j"$(nproc)" || exit 1

launch_script=._launch_debug.sh

cat << EOF > ${launch_script} && chmod +x ${launch_script}
#!/usr/bin/env sh
./scripts/qemu.py \\
    scripts/qemus/qemu_x86_dev.json \\
    --qemu-dir "${QEMU_DIR}" \\
    -v QMPORT=${hmp_port} \\
    -v GDB_PORT=${gdb_port} \\
    -v ROOTFS=lunaix_rootfs.ext2 \\
    -v ARCH=${ARCH} \\
    -v KBIN=build/bin/kernel.bin \\
    -v "KCMD=${default_cmd} rootfs=/dev/block/sda init=/bin/init" \\
    -- \\
    -nographic || exit 1 &

AUTOQEMU_DAEMON=\$!
echo "autoqemu daemon launched (pid=\$AUTOQEMU_DAEMON)"

gdb-multiarch \\
    build/bin/kernel.bin \\
    -ex "target remote localhost:${gdb_port}"

if ps -p \$AUTOQEMU_DAEMON > /dev/null
then
   kill -9 \$AUTOQEMU_DAEMON
fi
EOF

echo "debugger launch script written to: ${launch_script}"

if [[ -z "${TMUX}" ]]; then
    echo "for best effect, use tmux"
    GDB_NOSPLIT=1 "./${launch_script}"
else
    tmux new-window -a -n "[lunaix-debug]" -e "QMPORT=${hmp_port}" "./${launch_script}"
fi
