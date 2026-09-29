#!/bin/sh
# 9xOS build entry point: fetches the pinned Buildroot, configures it with the
# 9xOS external tree, and builds the image. Plain Buildroot underneath - you can
# always run "make -C <out>" yourself afterwards.
#
#   ./build.sh                          # build the default reference image
#   ./build.sh --profile secaudit       # add the security-audit tool profile
#   ./build.sh menuconfig               # any Buildroot make target
#   ./build.sh --defconfig <name> ...   # another defconfig from buildroot-external/configs
#   ./build.sh --reconfigure            # re-apply the defconfig (drops menuconfig edits)
set -eu

# --- Pinned inputs (change only via reviewed commits) ---------------------
BUILDROOT_TAG="2026.02.3"                                       # LTS
BUILDROOT_COMMIT="679b9ead7620bbf193620d1ebf56f53c1764d37a"
BUILDROOT_URL="https://gitlab.com/buildroot.org/buildroot.git"

TOP="$(cd "$(dirname "$0")" && pwd)"
EXTERNAL="$TOP/buildroot-external"
CACHE="${NINEXOS_CACHE:-$HOME/.cache/9xos}"
BR_DIR="$CACHE/buildroot-$BUILDROOT_TAG"
DEFCONFIG="9xos_bhargav_qemu_aarch64_dev_defconfig"
PROFILES=""
OUT=""
TARGETS=""
RECONFIGURE=0

usage() {
	sed -n '2,11p' "$0" | sed 's/^# \{0,1\}//'
	exit "${1:-0}"
}

while [ $# -gt 0 ]; do
	case "$1" in
		--defconfig) DEFCONFIG="$2"; shift 2 ;;
		--profile)   PROFILES="$PROFILES $2"; shift 2 ;;
		--out)       OUT="$2"; shift 2 ;;
		--reconfigure) RECONFIGURE=1; shift ;;
		-h|--help)   usage 0 ;;
		-*)          echo "unknown option: $1" >&2; usage 1 ;;
		*)           TARGETS="$TARGETS $1"; shift ;;
	esac
done

[ -f "$EXTERNAL/configs/$DEFCONFIG" ] || { echo "no such defconfig: $DEFCONFIG" >&2; exit 1; }
PROFILE_TAG="$(echo "$PROFILES" | tr ' ' '+' | sed 's/^+//')"
OUT="${OUT:-$TOP/out/${DEFCONFIG%_defconfig}${PROFILE_TAG:+-$PROFILE_TAG}}"

# --- Fetch and verify Buildroot -------------------------------------------
if [ ! -d "$BR_DIR/.git" ]; then
	mkdir -p "$CACHE"
	git clone --depth 1 --branch "$BUILDROOT_TAG" "$BUILDROOT_URL" "$BR_DIR"
fi
actual="$(git -C "$BR_DIR" rev-parse HEAD)"
if [ "$actual" != "$BUILDROOT_COMMIT" ]; then
	echo "Buildroot at $BR_DIR is $actual, expected $BUILDROOT_COMMIT ($BUILDROOT_TAG)." >&2
	echo "Refusing to build from unpinned sources. Remove that directory and retry." >&2
	exit 1
fi

# --- Configure (base defconfig + optional profiles) -----------------------
mkdir -p "$OUT"
if [ ! -f "$OUT/.config" ] || [ -n "$PROFILES" ] || [ "$RECONFIGURE" -eq 1 ]; then
	merged="$OUT/9xos_merged_defconfig"
	cat "$EXTERNAL/configs/$DEFCONFIG" > "$merged"
	for p in $PROFILES; do
		frag="$EXTERNAL/profiles/$p.fragment"
		[ -f "$frag" ] || { echo "no such profile: $p" >&2; exit 1; }
		echo "==> profile: $p"
		cat "$frag" >> "$merged"
	done
	make -C "$BR_DIR" O="$OUT" BR2_EXTERNAL="$EXTERNAL" BR2_DEFCONFIG="$merged" defconfig
fi

# --- Build ------------------------------------------------------------------
NINEXOS_GIT_REV="$(git -C "$TOP" describe --always --dirty 2>/dev/null || echo unknown)"
NINEXOS_BUILDROOT_VERSION="$BUILDROOT_TAG ($BUILDROOT_COMMIT)"
export NINEXOS_GIT_REV NINEXOS_BUILDROOT_VERSION

# shellcheck disable=SC2086 # word splitting of targets is intended
make -C "$OUT" $TARGETS

if [ -z "$TARGETS" ]; then
	echo
	echo "9xOS 0.1 (Bhargav) built: $OUT/images"
	echo "Boot it with:  $OUT/images/start-qemu.sh"
fi
