#!/usr/bin/env bash

sym_types=$1
bin=$2

nm_out=$(nm -nfbsd "$bin")
allsyms=($nm_out)
allsyms_len=${#allsyms[@]}

syms_idx=()

for (( i=0; i<allsyms_len; i+=3));
do
    type=${allsyms[$i + 1]}

    if [[ "$sym_types" == *"$type"* ]]; then
        syms_idx+=($i)
    fi
done

syms_len=${#syms_idx[@]}
declare -A assoc_array

echo '.section .ksymtable, "a", @progbits'
echo "    .global __lunaix_ksymtable"
echo "    __lunaix_ksymtable:"
echo "        .long $syms_len"
echo "        .align 8"

for i in "${syms_idx[@]}"
do
    addr=${allsyms[$i]}
    type=${allsyms[$i + 1]}
    sym=${allsyms[$i + 2]}

    echo "$(cat <<EOF
        .long 0x$addr
        .long __S$sym
        .align 8

EOF
)"
    assoc_array["$sym"]=1
done

for sym_str in "${!assoc_array[@]}"
do
    echo "$(cat <<EOF
    __S$sym_str:
        .asciz "$sym_str"
        .align 8
EOF
)"
done