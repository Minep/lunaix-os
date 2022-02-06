#!/usr/bin/bash

export _OS_NAME=$1

echo $(cat GRUB_TEMPLATE | envsubst)