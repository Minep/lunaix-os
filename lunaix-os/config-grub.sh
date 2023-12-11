#!/usr/bin/bash

cat GRUB_TEMPLATE | envsubst > "$1"