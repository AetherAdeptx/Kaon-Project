#!/bin/bash
set -e

echo "=== Building Kaon ==="

echo "[1/3] Assembling Boot..."
nasm -f bin Boot.asm -o Boot.bin

echo "[2/3] Assembling Boot2..."
nasm -f bin Boot2.asm -o Boot2.bin

echo "[3/3] Building disk image..."

# Boot2 occupies 4 sectors
truncate -s 2048 Boot2.bin

# Create a 1.44 MB floppy image
dd if=/dev/zero of=Kaon.img bs=512 count=2880 status=none

# Write boot sector
dd if=Boot.bin of=Kaon.img bs=512 seek=0 conv=notrunc status=none

# Write Stage 2
dd if=Boot2.bin of=Kaon.img bs=512 seek=1 conv=notrunc status=none

echo
echo "=== Build complete ==="
stat -c '%n: %s bytes' Boot.bin Boot2.bin Kaon.img

echo
echo "Boot signature:"
xxd -s 510 -l 2 Boot.bin

echo
echo "Stage 2 sectors:"
du -b Boot2.bin
