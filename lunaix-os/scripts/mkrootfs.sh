#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
WS=$(realpath $SCRIPT_DIR/..)
USR="${WS}/usr/build"

fs="ext2"
rootfs="${WS}/lunaix_rootfs.${fs}"
size_mb=16

if [ ! -d "${USR}" ]; then
    echo "build the user target first!"
    exit 1
fi

prefix=""
if [ ! "$EUID" -eq 0 ]; then
    echo "==================="
    echo "   mkrootfs require root privilege to manipulate disk image"
    echo "   you maybe prompted for password"
    echo "==================="
    echo
    prefix="sudo"
fi

tmp_mnt="$(mktemp -d)"

function cleanup() {
    echo "an error occured, reverting..."
    
    for arg in "$@"
    do
        case "$arg" in
            "tmpmnt")
                echo "revert: ${tmp_mnt}"
                rm -rf "${tmp_mnt}"
            ;;
            "img")
                echo "revert: ${rootfs}"
                rm -f "${rootfs}"
            ;;
        esac
    done
    
    exit 1
}

dd if=/dev/zero of="${rootfs}" count=${size_mb} bs=1M \
    || cleanup tmpmnt

mkfs.${fs} -L lunaix-rootfs -r 0 "${rootfs}" \
    || cleanup tmpmnt img

${prefix} mount -o loop "${rootfs}" "${tmp_mnt}" \
    || cleanup tmpmnt img

${prefix} chmod -R o+rwx ${tmp_mnt} \
    || cleanup tmpmnt img

cd "${tmp_mnt}" || cleanup tmpmnt img


mkdir bin dev sys task mnt lib usr \
    || has_err=1
mkdir -p usr/include \
    || has_err=1
mkdir -p usr/include/lunalibc \
    || has_err=1

cp "${USR}"/bin/* bin/ \
    || has_err=1
cp -R "${USR}"/includes/* usr/include/lunalibc \
    || has_err=1
mv bin/init . \
    || has_err=1

${prefix} chmod -R 775 . \
    || has_err=1
${prefix} chown -R root:root . \
    || has_err=1

sync -f .

cd "${WS}" || cleanup

${prefix} umount "${tmp_mnt}" || cleanup

${prefix} rm -d "${tmp_mnt}" || cleanup

if [ ! "${has_err:-0}" -eq 0  ]; then
    echo "done, but with error."
else
    echo "done"
fi

exit 0