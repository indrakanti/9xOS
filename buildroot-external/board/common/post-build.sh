#!/bin/sh
# 9xOS post-build: branding and build provenance.
# Called by Buildroot with $1 = target directory.
set -eu

TARGET_DIR="$1"
NINEXOS_VERSION="0.1"
NINEXOS_CODENAME="bhargav"

cat > "$TARGET_DIR/usr/lib/os-release" <<EOF
NAME="9xOS"
ID=9xos
PRETTY_NAME="9xOS ${NINEXOS_VERSION} (Bhargav)"
VERSION="${NINEXOS_VERSION} (Bhargav)"
VERSION_ID=${NINEXOS_VERSION}
VERSION_CODENAME=${NINEXOS_CODENAME}
BUILD_ID=${NINEXOS_GIT_REV:-dev}
EOF
ln -sf ../usr/lib/os-release "$TARGET_DIR/etc/os-release"

# Record exactly what went into this image (inputs for the safety case).
mkdir -p "$TARGET_DIR/usr/share/9xos"
{
	echo "ninexos_version=${NINEXOS_VERSION}"
	echo "ninexos_codename=${NINEXOS_CODENAME}"
	echo "ninexos_git=${NINEXOS_GIT_REV:-unknown}"
	echo "buildroot_version=${NINEXOS_BUILDROOT_VERSION:-unknown}"
	echo "defconfig=${BR2_CONFIG:-unknown}"
} > "$TARGET_DIR/usr/share/9xos/build-info"
