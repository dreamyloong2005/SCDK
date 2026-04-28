# SCDK Milestone 30 Architecture Review

Milestone 30 is a stop-and-audit milestone after the M11-M29 feature line. It
does not introduce PCI, real disk, networking, SMP, a shell, or POSIX
compatibility.

## What Is Working

- The boot path reaches a Limine-loaded x86_64 higher-half kernel and logs
  through COM1 serial.
- Core object metadata and capability table lookups reject invalid, missing,
  stale, and revoked capabilities.
- Endpoint/message dispatch is the control-plane boundary for console, tmpfs,
  VFS, proc, grant-test, and devmgr services.
- Ring/grant paths are present for batched console data-plane writes from user
  code.
- Initrd, tmpfs, VFS, flat loader, proc spawn, user grants, user rings,
  capability revocation, user fault handling, timer preemption, and devmgr fake
  device queues all have boot-time self-tests.
- Direct console hardware access is isolated to early boot, panic/log fallback,
  arch drivers, and the console backend.

## What Is Stubbed

- The device manager registers only a fake device and fake queue.
- VFS routes to the initrd-backed tmpfs service only.
- The executable loader supports flat binaries with a fixed load model.
- Fault handling kills user tasks; it does not provide user exception delivery.
- Timer preemption is single-core PIT based.
- Grants and rings are static-table prototypes.

## What Is Unsafe Or Temporary

- Services still run in kernel space, although their interfaces are shaped as
  endpoint/message/ring/grant boundaries.
- Kernel bootstrap tests still use some controlled in-kernel buffers before a
  fully general user service model exists.
- There is no full derivation tree for capability revoke.
- There is no SMP, IOMMU, DMA isolation, waitset, or blocking async runtime.
- Object lifetime cleanup is intentionally narrow and test-oriented.

## What Was Fixed During M30

- Added explicit M30 documentation for architecture state and invariants.
- Added `tools/arch-check.sh` to repeat the main source-level architecture
  checks.
- Added boot-time M30 review markers after all M0-M29 self-tests.
- Updated the final boot marker to `milestone 30 complete`.

## Before M31+

- Keep devmgr work at the fake device/queue authorization layer until the M30
  invariants stay clean.
- Add any next feature through object, capability, endpoint/message, and
  ring/grant boundaries.
- Do not add PCI, real disk, networking, SMP, shell, or POSIX compatibility
  until the service boundary and cleanup model are reviewed again.
- Expand `tools/arch-check.sh` before adding new hardware-facing or user-pointer
  paths.
