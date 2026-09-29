# 9xOS 0.1 "Bhargav" – Safety Scope (DRAFT)

> **Status:** draft for internal review. 9xOS 0.1 is a prototype. It is **not**
> ISO 26262 compliant or assessed, and no ASIL is claimed. This document
> fixes the scope and structure the safety case will grow into.

## 1. Approach

9xOS is developed as a **Safety Element out of Context (SEooC)** following the
ISO 26262-10 pattern: we state assumed safety goals and **Assumptions of Use
(AoU)**; the integrator validates them against their real item and HARA.

Linux itself is treated the way the Linux Foundation **ELISA** project
recommends: we do not claim the whole kernel is safe. We keep the
safety-relevant path small, monitor it, and make every failure end in a
defined safe state.

## 2. Element scope for 0.1

| In scope | Out of scope |
|---|---|
| Garuda liveness monitor (`components/garuda`) | Customer applications |
| Kernel fail-safe configuration (panic/oops/hang → reset) | Real-time guarantees (no PREEMPT_RT yet) |
| Hardware-watchdog-based safe state | Qualification of the Linux kernel itself |
| Build reproducibility inputs (pinned Buildroot, kernel, hashes) | Security-audit profile (`secaudit`) – never in a safety image |

## 3. Assumed safety goal

**SG-01** – If a supervised application stops executing its cycle, the system
shall reach the safe state (hardware reset) within
`period_ms + feed_interval + watchdog_timeout`.

## 4. Safety requirements and traceability

| ID | Requirement | Code | Verification |
|---|---|---|---|
| GRD-REQ-01 | A registered client that does not kick within `period_ms` is faulted. | `monitor.c: gm_check`, `do_kick` | `test_deadline`, `test_late_kick`, `it_heartbeat.sh` test 1 |
| GRD-REQ-02 | Faults latch; kicks and re-registration cannot clear them. Only an explicit unregister by the owner does. | `do_kick`, `do_register` | `test_latch` |
| GRD-REQ-03 | Only the PID that registered a name may kick, re-register or unregister it. PID comes from kernel `SCM_CREDENTIALS`, never from the payload. | `do_*`, `garudad.c: receive` | `test_pid` |
| GRD-REQ-04 | Kick sequence numbers must strictly increase (wrap-safe). Stale or replayed kicks are rejected. | `do_kick` | `test_seq` |
| GRD-REQ-05 | Malformed messages are rejected and never change monitor state. | `gm_handle`, `receive` | `test_invalid` |
| GRD-REQ-06 | The system is healthy only if no client is faulted. The watchdog is fed only while healthy. | `gm_healthy`, `garudad.c` main loop | `test_capacity_and_health`, `it_heartbeat.sh` |
| KRN-REQ-01 | Kernel oops, panic, soft lockup and hung tasks end in an immediate reboot. | `linux-safety-security.fragment`, `90-9xos-hardening.conf` | Target test – **open** |
| KRN-REQ-02 | The watchdog cannot be disarmed from user space (`NOWAYOUT`). | `linux-safety-security.fragment` | Target test – **open** |

## 5. Assumptions of Use (integrator obligations)

- **AoU-01** The hardware watchdog is independent of the main CPU clock and
  power domain, and its timeout is configured by the integrator.
- **AoU-02** The kernel is built with the 9xOS fragment unchanged, in
  particular `CONFIG_WATCHDOG_NOWAYOUT=y`.
- **AoU-03** Each safety-relevant application registers with Garuda and kicks
  only after it has completed a correct cycle, not from a separate thread.
- **AoU-04** `period_ms` is chosen from the application's fault tolerant time
  interval (FTTI): `period_ms + feed + watchdog timeout < FTTI`.
- **AoU-05** A hardware reset is an acceptable safe state for the item. If it
  is not, the integrator provides a different safe-state mechanism.
- **AoU-06** Access to `/run/garuda.sock` is limited to trusted
  applications (socket mode 0660, owner + group).
- **AoU-07** For ASIL B and above, the integrator adds an **external monitor**
  (separate MCU) that supervises garudad itself. Liveness monitoring on the
  same Linux instance is not sufficient on its own.

## 6. Known gaps (before any safety claim)

1. Garuda checks liveness only. It does not check logical program flow or
   deadline arrival order. Planned: program-flow monitoring (control-flow
   checkpoints).
2. There is no external MCU monitor yet (AoU-07). Planned for 9xOS 1.1.
3. There are no target tests for KRN-REQ-01/02 yet. They need a QEMU boot
   test in CI.
4. Tool confidence (ISO 26262-8 clause 11) for GCC, CMake and Buildroot is
   not assessed.
5. There is no ASPICE process evidence yet (reviews, change control, CM).
6. The kernel is mainline 6.18 LTS. Moving to a **CIP** (Civil Infrastructure
   Platform) SLTS kernel is recommended for 10-year maintenance.
