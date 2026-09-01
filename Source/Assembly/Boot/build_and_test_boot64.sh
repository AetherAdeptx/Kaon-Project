#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source_file="${script_dir}/Boot64.asm"
build_dir="${script_dir}/build"
boot_image="${build_dir}/Boot64.bin"
console_log="${build_dir}/Boot64.console.log"
bios_log="${build_dir}/Boot64.bios.log"
qemu_log="${build_dir}/Boot64.qemu.log"
screen_dump="${build_dir}/Boot64.screen.ppm"
screen_image="${build_dir}/Boot64.screen.png"
expected_output="RPLKAON"

nasm_command="${NASM:-nasm}"
qemu_command="${QEMU:-qemu-system-x86_64}"
magick_command="${MAGICK:-magick}"

for command_name in "${nasm_command}" "${qemu_command}" "${magick_command}" timeout; do
    if ! command -v "${command_name}" >/dev/null 2>&1; then
        printf 'ERROR: required command not found: %s\n' "${command_name}" >&2
        exit 1
    fi
done

mkdir -p "${build_dir}"

printf '[1/3] Assembling %s\n' "${source_file}"
"${nasm_command}" -Wall -f bin "${source_file}" -o "${boot_image}"

printf '[2/3] Checking boot-sector structure\n'
image_size="$(stat -c '%s' "${boot_image}")"
if [[ "${image_size}" != '512' ]]; then
    printf 'ERROR: expected 512 bytes, got %s bytes\n' "${image_size}" >&2
    exit 1
fi

boot_signature="$(od -An -tx1 -j510 -N2 "${boot_image}" | tr -d '[:space:]')"
if [[ "${boot_signature}" != '55aa' ]]; then
    printf 'ERROR: expected boot signature 55aa, got %s\n' "${boot_signature}" >&2
    exit 1
fi

printf '[3/3] Booting with QEMU and capturing serial/BIOS/VGA output\n'
set +e
(
    cd "${build_dir}"
    {
        sleep 2
        printf 'screendump Boot64.screen.ppm\n'
        sleep 0.25
        printf 'quit\n'
    } | timeout --signal=TERM 5s "${qemu_command}" \
        -drive 'format=raw,file=Boot64.bin' \
        -display none \
        -monitor stdio \
        -serial file:Boot64.console.log \
        -debugcon file:Boot64.bios.log \
        -global isa-debugcon.iobase=0x402 \
        -no-reboot \
        -no-shutdown
) >"${qemu_log}" 2>&1
qemu_status=$?
set -e

if [[ ! -s "${console_log}" ]]; then
    printf 'ERROR: QEMU did not produce a serial console log\n' >&2
    exit 1
fi

if [[ ! -s "${bios_log}" ]] || ! grep -q 'Booting from 0000:7c00' "${bios_log}"; then
    printf 'ERROR: QEMU did not capture the expected SeaBIOS boot messages\n' >&2
    exit 1
fi

if [[ ! -s "${screen_dump}" ]]; then
    printf 'ERROR: QEMU did not produce the BIOS/VGA screen capture\n' >&2
    exit 1
fi

"${magick_command}" "${screen_dump}" "${screen_image}"
qemu_output="$(<"${console_log}")"
printf 'Serial output: %s\n' "${qemu_output}"

if [[ "${qemu_output}" != *"${expected_output}"* ]]; then
    printf 'ERROR: expected boot output to contain %s\n' "${expected_output}" >&2
    exit 1
fi

if (( qemu_status != 0 && qemu_status != 124 )); then
    printf 'ERROR: QEMU exited with status %s\n' "${qemu_status}" >&2
    exit 1
fi

printf 'PASS: Boot64 entered real, protected, and 64-bit modes (%s).\n' "${expected_output}"
printf 'Saved console log: %s\n' "${console_log}"
printf 'Saved SeaBIOS log: %s\n' "${bios_log}"
printf 'Saved BIOS/VGA screen: %s\n' "${screen_image}"
