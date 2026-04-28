# SCDK Milestone 30 Invariants

These invariants define the architecture line at the end of M30.

## Kernel Boundary

- SCDK is not a UNIX clone.
- POSIX compatibility is not a native kernel API.
- Kernel code remains freestanding and does not include libc or host OS
  headers.

## Object And Capability Boundary

- Cross-subsystem access must be represented by object IDs and capabilities.
- New objects must carry an `owner_core`.
- Public operations that access objects must check capability type and rights.
- Revoked or stale capabilities must fail future lookup and rights checks.

## Service Boundary

- Services expose endpoints and messages as their control-plane interface.
- Services must not directly mutate another service's private structs.
- A kernel-resident service must still keep an interface that can later move to
  user space.

## Data-Plane Boundary

- High-frequency operations should use rings and grants where practical.
- A ring must have a checked bound target endpoint before service processing.
- User payload access by services must go through grants or validated user-copy
  helpers, not raw user pointer dereferences.
- Grants must check source task, target endpoint, rights, bounds, lifetime, and
  revocation state.

## Hardware Boundary

- Direct serial/framebuffer writes are allowed only for early boot, panic/log
  fallback, architecture drivers, and the console backend.
- Normal post-console output should route through the console endpoint/message
  or ring/grant frontend paths.

## Fault Boundary

- User faults kill or mark only the offending task.
- Kernel faults still panic with diagnostics.
- Bad user pointers and invalid syscalls must route through explicit fault
  handling rather than silent failure.

## Feature Boundary

- Disk drivers, networking, SMP, POSIX compatibility, shell, GUI, package
  manager, and full user-space remain out of scope at M30.
