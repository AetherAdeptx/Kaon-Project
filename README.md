# Kaon

Kaon boots through a small BIOS/NASM loader, enters x86-64 long mode, and
hands control to a freestanding C kernel. No host C library is linked into the
kernel.

## Build and run

The local development tools are NASM, GCC, GNU binutils, Make, and QEMU. Build
and perform a headless boot test with:

```sh
make
make test
```

Use `make run` for the QEMU window or `make debug` to start QEMU paused with a
GDB server on `localhost:1234`. Generated objects, maps, binaries, and the disk
image are kept under `build/`.

Kernel C code lives in `Source/C/Kaon_System/`; public kernel headers live in its
`include/` directory. Keep hardware entry and CPU-mode transitions in
`Source/Assembly/`, then expose small C interfaces for the rest of the kernel.

This first BIOS loader can load a kernel of at most 127 sectors (65,024 bytes)
at physical address `0x10000`. That is enough for the initial kernel; replace
the bootstrap disk reader when Kaon gains its own storage and memory managers.

The bootstrap page tables identity-map 8 MiB reserved for early boot using
2 MiB pages, followed by a 256 MiB region for Librarian at physical and virtual
address `0x800000` using 65,536 standard 4 KiB pages. Librarian's 128 page
tables occupy 512 KiB starting at physical address `0x100000` in boot memory.
The loader passes these boundaries through the assembly entry stub into C, so
Librarian uses the bootloader's authoritative layout rather than copied values.

Librarian stores its allocator metadata at the beginning of its managed region:
an 8 KiB in-use bitmap followed by a 512 KiB page-index-to-physical-address
table. These structures reserve 130 pages, leaving 65,406 allocatable pages.

Persistent kernel globals are grouped into the `.kaon_globals` linker section.
`Config.h` contains Librarian's assignment-start and assignment-end offsets;
both are relative to the 256 MiB managed region and must be 4 KiB aligned.
Librarian can allocate either one page or a contiguous block of pages, and it
will never assign a block outside that configured window.

The PS/2 keyboard watcher currently runs by polling from the kernel loop. It
maintains a 256-entry key-state array and a 128-event ring buffer that other
kernel modules can query without depending on the console. Console drawing is
controlled by `KAON_CONSOLE_DRAW_ENABLED` and is enabled by default.

The console provides configurable user-name and current-directory fields, a
256-line transcript, 128-byte variable-length input lines, Shift+Enter or
Shift+Space line breaks, command submission with Enter, Up/Down command-history
navigation with draft restoration, and a polling-loop cursor heartbeat.
The most recent 100 commands are stored as fixed 128-byte records with lengths
and a checksum. They are loaded from and flushed to 32 raw ATA sectors starting
at LBA 128; this reserved range follows the loader's maximum kernel range and
will be replaced by a filesystem-backed history file later.
