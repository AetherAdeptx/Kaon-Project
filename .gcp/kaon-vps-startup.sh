#!/usr/bin/env bash
set -Eeuo pipefail

export DEBIAN_FRONTEND=noninteractive

# The Free Tier e2-micro has 1 GiB of RAM. A small swap file makes package
# installation and occasional compiler/linker spikes more reliable.
if [[ ! -e /swapfile ]]; then
    fallocate -l 2G /swapfile
    chmod 0600 /swapfile
    mkswap /swapfile
    swapon /swapfile
    printf '%s\n' '/swapfile none swap sw 0 0' >>/etc/fstab
fi

apt-get update
apt-get install --yes --no-install-recommends \
    bash-completion \
    binutils \
    build-essential \
    ca-certificates \
    clang \
    curl \
    dosfstools \
    file \
    gdb \
    git \
    jq \
    less \
    make \
    mtools \
    nasm \
    nginx \
    qemu-system-x86 \
    qemu-utils \
    ripgrep \
    rsync \
    shellcheck \
    tmux \
    unzip \
    vim \
    xorriso \
    xxd \
    zip

cat >/etc/ssh/sshd_config.d/99-kaon-security.conf <<'EOF'
PasswordAuthentication no
KbdInteractiveAuthentication no
PermitRootLogin no
EOF

sshd -t
systemctl reload ssh
systemctl enable --now nginx

install -d -m 0755 /var/lib/kaon
date --iso-8601=seconds >/var/lib/kaon/provisioned-at
