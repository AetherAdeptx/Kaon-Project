#!/bin/bash

rm -f qemu.log

echo "=== Starting Kaon under QEMU ==="
echo "QEMU log: qemu.log"
echo

qemu-system-x86_64 \
    -drive format=raw,file=Kaon.img \
    -boot c \
    -no-reboot \
    -no-shutdown \
    -d int,cpu_reset,guest_errors \
    -D qemu.log
