#!/usr/bin/bash

export _OS_NAME=$1

cat GRUB_TEMPLATE | envsubst > "$2"