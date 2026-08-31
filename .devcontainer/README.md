# Kaon development lab

This directory defines an isolated Debian 13 development container for Kaon.
The standard `devcontainer.json` entry point can be opened by compatible cloud
development services and desktop editors. Cloud hosts generally do not expose
KVM, so QEMU will use software emulation there.

## Cloud development

Open the repository in a development-container-compatible cloud workspace. The
workspace builds `.devcontainer/Containerfile` automatically and connects as
the unprivileged `developer` user. NASM, GCC, Clang, GDB, QEMU, Rust, Go, Java,
Node.js, and the supporting OS-development tools are included in the image.

## Local Podman development

It runs rootlessly under Podman and has access only to the Kaon project, its
own persistent home volume, and the explicitly passed KVM, DRM, and TUN
devices that exist on the host. Its dedicated Podman storage root defaults to
`/var/mnt/Storage/KaonLab/podman`, keeping the image, container layer, and home
volume on the mechanical drive rather than the system SSD.

Start an interactive shell:

```bash
.devcontainer/kaon-lab shell
```

Run a single command:

```bash
.devcontainer/kaon-lab run qemu-system-x86_64 --version
```

Build an assembly source without overwriting an existing output:

```bash
.devcontainer/kaon-asm Source/Assembly/Boot/Boot.asm /tmp/Boot.bin
```

Back up the reproducible configuration, container metadata, and persistent
developer home volume under `~/ChatGPT Temp/Backups`:

```bash
.devcontainer/kaon-backup
```

Transfer files across the container boundary without overwriting existing
destinations:

```bash
.devcontainer/kaon-export /tmp/Boot.bin /tmp/Boot.bin
.devcontainer/kaon-import /tmp/example.txt /home/developer/example.txt
```

The defaults reserve eight CPU threads and up to 10 GiB of memory. Override
them when initially creating the container with `KAON_LAB_CPUS`,
`KAON_LAB_MEMORY`, and `KAON_LAB_MEMORY_SWAP`. The storage location can be
overridden with `KAON_LAB_STORAGE_ROOT`.

`destroy` removes only the container. `reset` additionally deletes the
container's persistent home volume. Neither command removes project files.

QEMU's graphical frontends are intentionally not connected to the host
desktop. Prefer `-nographic`, `-display curses`, VNC, or QEMU on the host for
graphical guests. KVM acceleration is available inside the container.
