#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

declare -A nm_lookup

kbin="$1"

nm_out=($(nm -l "${kbin}" | awk '$3 ~ /^__lga*/ {print $3 "," $4}'))

for line in "${nm_out[@]}"
do
    parts=(${line//,/ })
    if [ "${parts[1]}" != "" ]; then
        path=(${parts[1]//:/ })
        relative=$(realpath -s --relative-to="${SCRIPT_DIR}/.." "${path[0]}")
        nm_lookup["${parts[0]}"]="${relative}"
    fi
done

objdump_out=($(objdump -t -j .lga "${kbin}" | grep .lga | awk '!($NF ~ /_start$|_end$|_ldorder$/) {print $NF}'))

list=()
dev_list=()
init_list=()
sysmap_list=()
init_prefix="lunainit_"
sysmap_prefix="twiplugin_inits_"
devdefs_prefix="devdefs_"
fs_prefix="fs_"

for line in "${objdump_out[@]}"
do
    loc=${nm_lookup["$line"]}
    if [ "$loc" == "" ]; then
        continue
    fi
    
    type="unknown"
    line=$(echo "$line" | awk '{ sub(/^__lga_/, ""); print }')

    if [[ $line == $init_prefix* ]]; then
        line=$(echo "$line" | awk "{ sub(/^${init_prefix}/, \"\"); print }")
        init_list+=("$line $loc")
        type="lunainit"
    fi
    
    if [[ $line == $sysmap_prefix* ]]; then
        line=$(echo "$line" | awk "{ sub(/^${sysmap_prefix}/, \"\"); print }")
        sysmap_list+=("$line $loc")
        type="twiplugin"
    fi

    if [[ $line == $fs_prefix* ]]; then
        line=$(echo "$line" | awk "{ sub(/^${fs_prefix}/, \"\"); print }")
        sysmap_list+=("$line $loc")
        type="fs"
    fi

    if [[ $line == $devdefs_prefix* ]]; then
        line=$(echo "$line" | awk "{ sub(/^${devdefs_prefix}/, \"\"); print }")
        type="devdefs"
    fi

    line=$(echo "$line" | awk "{ sub(/_on_[a-z]+$/, \"\"); print }")
    list+=("$line $loc")

    echo "| $type | \`$line\` | $loc |"
done

