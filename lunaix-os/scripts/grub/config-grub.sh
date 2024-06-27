#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

cat "${SCRIPT_DIR}/GRUB_TEMPLATE" | envsubst > "$1"