#!/bin/bash

declare qemu_cmd=(
    "-s -S"
    "-smp 1"
    "-rtc base=utc"
    "-no-reboot"
    "-machine q35"
    "-cpu pentium3,rdrand"
    "-no-shutdown"
    "-d cpu_reset"
    "-d trace:ide_dma_cb"
    "-vga std,retrace=precise"
    "-serial telnet::12345,server,nowait,logfile=lunaix_ttyS0.log"
    "-device ahci,id=ahci"
)

function set_memory {
    qemu_cmd+=("-m ${1}")
}

function set_cdbootable {
    name=$1
    file=$2
    qemu_cmd+=(
        "-drive id=$name,file='$file',readonly=on,if=none,format=raw"
        "-device ide-cd,drive=$name,bus=ahci.0"
    )
}

function set_drive_1 {
    name=$1
    file=$2
    qemu_cmd+=(
        "-drive id=$name,file='$file',if=none,format=raw"
        "-device ide-hd,drive=$name,bus=ahci.1"
    )
}

function set_monitor {
    if [ "${1}" == "" ]; then
        qemu_cmd+=(
            "-monitor stdio"
        )
    else
        qemu_cmd+=(
            "-monitor telnet::${1},server,nowait,logfile=qm.log"
        )
    fi
}

VM_MEM=1G
CD_ROM=
DISK=
HMP_PORT=12345
QEMU_EXEC="qemu-system-i386"

while true; do
    case "$1" in
        -m | --memory ) VM_MEM="$2"; shift 2 ;;
        --cd-rom ) CD_ROM="$2"; shift 2 ;;
        --hmp ) HMP_PORT="$2"; shift 2 ;;
        --hdd ) DISK="$2"; shift 2 ;;
        *) break;;
    esac
done

if [ "$CD_ROM" == "" ]; then
    echo "error: no bootable image"
    exit 1
fi

if [ "$QEMU_PATH" != "" ]; then
    QEMU_EXEC="$QEMU_PATH"
fi

set_memory "$VM_MEM"
set_cdbootable "cdrom" "$CD_ROM"
set_monitor "$HMP_PORT"

if [ "$DISK" != "" ]; then
    set_drive_1 "hdd1" "$DISK"
fi

CMD="${QEMU_EXEC} ${qemu_cmd[@]}"

echo $CMD

eval "$CMD"