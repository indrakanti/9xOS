# 9xOS

**Linux-based safety operating system** – built from open-source Linux,
structured for ISO 26262, and hardened for security.

Current release: **9xOS 0.1 "Bhargav"** (prototype).

## What's inside

| Layer | Tool | Where |
|---|---|---|
| Base image (kernel, toolchain, root filesystem) | Buildroot 2026.02.3 LTS, pinned | `buildroot-external/` |
| 9xOS components (Garuda safety monitor, more later) | CMake | `components/` |
| Kernel safety and security settings | Kconfig fragment | `buildroot-external/board/common/` |
| Runtime hardening | sysctl | `buildroot-external/board/common/rootfs-overlay/` |
| Security-audit tools (opt-in) | Buildroot profile | `buildroot-external/profiles/secaudit.fragment` |
| Safety documentation | Markdown | `docs/safety/` |

Everything is plain, traditional tooling. Buildroot is Make and Kconfig, and
the components are ordinary CMake projects. Customers can use the prebuilt
image, rebuild it, or pull a single component into their own build.

## Garuda – liveness monitor

Applications register with `garudad` and send a heartbeat ("kick") every cycle.
`garudad` feeds the hardware watchdog **only while every client is healthy**.
If any client misses its deadline, the fault latches, feeding stops, and the
board resets into its safe state.

```c
#include <garuda/garuda_client.h>

garuda_client_t g;
garuda_client_open(&g, NULL);
garuda_register(&g, "perception", 100);   /* must kick at least every 100 ms */
for (;;) {
    run_cycle();
    garuda_kick(&g);
}
```

Use it from your own build:

```cmake
find_package(Garuda 0.1 REQUIRED)
target_link_libraries(my_app PRIVATE 9xos::garuda_client)
```

or `pkg-config --cflags --libs garuda`.

## Build and test the components (host)

```sh
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Build the OS image

On a Linux host with the [Buildroot prerequisites](https://buildroot.org/downloads/manual/manual.html#requirement),
or inside `tools/docker`:

```sh
./build.sh                       # QEMU aarch64 reference image
./build.sh --profile secaudit    # plus security-audit tools (dev/assessment only)
./build.sh menuconfig            # any Buildroot target
out/9xos_bhargav_qemu_aarch64_dev/images/start-qemu.sh
```

`build.sh` refuses to build if the Buildroot checkout doesn't match the pinned
commit.

## Security approach

9xOS takes its security posture from the hardening community (KSPP kernel
settings, sysctl lockdown, a minimal image) rather than from a pentest distro.
The Kali-style assessment role is available as the opt-in `secaudit` profile
(nmap, tcpdump, lynis, audit, nftables, strace). It is never part of a
production or safety image.

## Safety status

9xOS 0.1 is a **prototype**. It is not ISO 26262 compliant or assessed, and
no ASIL is claimed. See
[`docs/safety/bhargav-0.1-safety-scope.md`](docs/safety/bhargav-0.1-safety-scope.md)
for the SEooC scope, requirements traceability, Assumptions of Use and known
gaps.

## Releases

| Version | Codename | Focus |
|---|---|---|
| 0.1 | Bhargav | Reference image, Garuda liveness monitor, safety scope draft |
| 1.0 | Charaka | *planned* |
