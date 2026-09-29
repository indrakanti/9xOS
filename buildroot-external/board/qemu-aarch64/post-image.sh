#!/bin/sh
# 9xOS: write a start script for the QEMU reference image next to the images.
# Unlike Buildroot's generic QEMU script, this adds the i6300esb watchdog
# that garudad feeds.
set -eu

cat > "$BINARIES_DIR/start-qemu.sh" <<'EOF'
#!/bin/sh
# Boot 9xOS 0.1 "Bhargav" in QEMU. Log in as root on the console.
# Exit QEMU with Ctrl-A then X.
HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"
QEMU="${QEMU:-$HERE/../host/bin/qemu-system-aarch64}"
[ -x "$QEMU" ] || QEMU=qemu-system-aarch64
exec "$QEMU" -M virt -cpu cortex-a53 -smp 2 -m 512M -nographic \
    -kernel Image \
    -append "rootwait root=/dev/vda console=ttyAMA0" \
    -drive file=rootfs.ext4,if=none,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0 \
    -netdev user,id=eth0 -device virtio-net-device,netdev=eth0 \
    -device i6300esb -action watchdog=reset \
    "$@"
EOF
chmod +x "$BINARIES_DIR/start-qemu.sh"
