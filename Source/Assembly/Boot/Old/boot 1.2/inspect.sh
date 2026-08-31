#!/bin/bash

echo "=== Kaon Image ==="
stat -c '%n: %s bytes' Boot.bin Boot2.bin Kaon.img

echo
echo "=== Boot Signature ==="
xxd -s 510 -l 2 Boot.bin

echo
echo "=== Boot Sector ==="
xxd -l 64 Boot.bin

echo
echo "=== Stage 2 Beginning ==="
xxd -l 64 Boot2.bin

echo
echo "=== Stage 2 Size ==="
du -b Boot2.bin
