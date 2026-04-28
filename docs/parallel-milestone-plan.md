# SCDK / ScadekOS Parallel Milestone Plan

This document describes the completed baseline and ongoing milestone structure for
SCDK and ScadekOS as a parallel kernel/OS development effort.

SCDK owns kernel mechanisms, isolation, hardware-facing paths, and enforcement.
ScadekOS owns OS integration, initrd payloads, userspace programs, service policy,
operator-visible behavior, and release validation.

## Current Baseline

- Completed milestone range: M0-M31
- SCDK kernel target: `0.4.0-alpha.3`
- ScadekOS target: `0.1.0-devpreview.3`
- Native interface model: objects, capabilities, endpoints, messages, rings, and grants
- Native non-goal: POSIX/UNIX as the kernel ABI
- Current interactive path: framebuffer text console + PS/2 keyboard + TTY endpoint + ScadekOS command runner
- Current filesystem path: initrd-backed tmpfs routed through VFS
- Current process path: flat user payload loading through the proc service

## Architecture Split

### SCDK Kernel

SCDK is the Sharded Capability Dataplane Kernel. Its kernel-side design is
capability-oriented and data-plane-first.

SCDK owns:

- x86_64 boot, memory discovery, paging, and low-level entry code
- early serial console, framebuffer backend, panic path, and kernel logging
- object IDs, capability tables, rights checks, stale generation rejection, and revocation
- endpoint and message dispatch
- grant validation for zero-copy payload transfer
- ring creation, binding, submission, and completion paths
- PMM, VMM, kernel heap, address spaces, tasks, threads, user mode, and syscall dispatch
- initrd parsing, tmpfs service, VFS routing, loader, proc service, session service, TTY service, timer, fault handling, and devmgr prototypes
- self-tests that prove architectural invariants at boot

SCDK does not own user-facing shell semantics, current working directory
behavior, command parsing, or application policy.

### ScadekOS

ScadekOS is the OS integration layer around SCDK. It packages the kernel,
initrd contents, userspace payloads, boot scripts, and validation flows into a
usable developer preview.

ScadekOS owns:

- release metadata and kernel version pinning
- initrd policy and boot script contents
- flat userspace payloads and `libscadek`
- `/init`, `/runner`, `/hello`, grant demo, ring demo, and prompt payloads
- command runner behavior: `help`, `version`, `clear`, `pwd`, `cd`, `ls`, `cat`, `run`, `services`, `caps`, `grants`, `rings`, and `exit`
- smoke tests for serial boot, no-serial framebuffer output, and keyboard input
- user-visible validation of VFS, proc, grants, rings, services, TTY, and version output

ScadekOS does not directly access serial ports, framebuffer memory, keyboard
controller ports, or kernel-private subsystem internals.

## Core Invariants

- All subsystem boundaries are expressed through objects, capabilities,
  endpoints, messages, rings, and grants.
- Services may currently run in kernel space, but their interfaces are shaped so
  they can later move toward user services or other deployment forms.
- Cold paths remain explicit, auditable, and capability checked.
- Hot paths use rings and grants where the prototype already benefits from
  batching or zero-copy transfer.
- Serial remains the early boot, debug, panic, and CI path.
- The framebuffer console and TTY path provide the local interactive developer
  console.
- POSIX compatibility, if added later, belongs above the native SCDK interfaces.

## Completed Milestones

### M0: Repository And Build Baseline

- Established the SCDK repository structure, freestanding build rules, linker
  script, Limine boot assets, and ISO generation path.
- Established ScadekOS as the packaging/integration layer with SCDK as its
  kernel dependency.

### M1: Boot, Serial, And Framebuffer Discovery

- Booted the higher-half x86_64 kernel through Limine.
- Brought up COM1 serial output and framebuffer discovery.
- Logged bootloader, memory map, executable address, and framebuffer facts.

### M2: Logging, Panic, And Early Diagnostics

- Added structured log categories and panic behavior.
- Kept serial output available for failure diagnosis and CI.
- Added framebuffer panic-path availability.

### M3: Object Manager And Capability Table

- Added typed kernel objects with generation-based IDs.
- Added capability handles with rights bits and object-type validation.
- Rejected missing rights, invalid objects, stale generations, and revoked caps.

### M4: Endpoints And Messages

- Added endpoint objects, endpoint capabilities, and message dispatch.
- Established service lookup/registration through endpoint messages.
- Used endpoints as the primary control-plane boundary between subsystems.

### M5: Ring Prototype

- Added ring objects, descriptors, completions, and endpoint binding.
- Validated ring rights and target endpoint authorization.
- Established the batched data-plane shape used by later console tests.

### M6: Grant Prototype

- Added grant objects and grant capabilities over memory ranges.
- Validated grant rights, offsets, lengths, bounds, and revoke behavior.
- Established zero-copy payload transfer as the native shared-memory model.

### M7: Console Service

- Added a console service endpoint.
- Routed console writes through endpoint messages and grant-backed data.
- Kept direct hardware access restricted to approved backend code.

### M8: Memory Map And PMM

- Parsed the boot memory map and initialized physical memory management.
- Added frame allocation/free tests and basic memory accounting.

### M9: VMM Skeleton

- Added virtual memory mapping primitives and early page-table handling.
- Preserved the higher-half kernel layout and HHDM-based access model.

### M10: Scheduler Placeholder

- Added scheduler and thread scaffolding.
- Created the initial task/thread lifecycle shape before full user execution.

### M11: Physical Frame Accounting

- Hardened PMM behavior around usable memory ranges and frame reuse.
- Added boot self-tests for allocation, free, and invalid operations.

### M12: Virtual Memory Mapping

- Expanded VMM mapping and permission checks.
- Added tests for map/unmap, duplicate mappings, and missing rights.

### M13: Kernel Heap

- Added the freestanding kernel heap.
- Validated allocation, alignment, reuse, and failure behavior.

### M14: Address Spaces

- Added address-space objects and capability-checked map operations.
- Separated address-space ownership from raw page-table mechanics.

### M15: Task And Thread Lifecycle

- Added user task and thread objects.
- Added lifecycle states, start/exit handling, and cleanup checks.

### M16: User Mode Entry

- Added x86_64 user-mode transition support.
- Established the flat user payload execution model used by early demos.

### M17: Syscall Dispatch

- Added the syscall entry and dispatcher.
- Supported native debug, endpoint call, yield, exit, grant, ring, and revoke paths.
- Rejected invalid syscall numbers and bad user pointers.

### M18: Initrd

- Added Limine module discovery and ustar initrd parsing.
- Loaded `/init`, `/hello`, demos, config files, and text assets from initrd.

### M19: Tmpfs Service

- Added initrd-backed tmpfs file serving.
- Supported open, read, close, and file-cap lifecycle behavior.

### M20: VFS Service

- Added VFS routing over tmpfs mounted at `/`.
- Kept VFS as a native endpoint/message service rather than a POSIX fd table.

### M21: Loader

- Added flat executable loading from VFS.
- Created user task/address-space/thread state for loaded payloads.

### M22: Proc Service

- Added process spawn through a proc service endpoint.
- Routed process creation through VFS and loader boundaries.

### M23: User Grants

- Exposed grant creation and revocation through syscalls.
- Validated service-side read/write checks, bounds rejection, and stale grant use.

### M24: User Rings

- Exposed user ring creation, binding, submission, and polling.
- Validated ring-backed console writes and completion batches.

### M25: Grant-Backed Console Data Path

- Exercised console output through grant-backed user payloads.
- Preserved capability checks across endpoint and grant boundaries.

### M26: Revocation

- Added capability revoke behavior and stale generation rejection.
- Validated revoke effects across core object/capability operations.

### M27: Fault Handling

- Added user page-fault, invalid-syscall, and bad-pointer handling.
- Killed faulting user tasks while keeping kernel faults as panic conditions.

### M28: Timer And Preemption Prototype

- Added timer initialization and tick handling.
- Exercised simple preemption paths for user threads.

### M29: Device Manager Prototype

- Added device and device-queue authorization shapes.
- Registered a fake device and validated queue-cap binding rights.
- Kept real disk, network, PCI, DMA, and IOMMU work out of the current baseline.

### M30: Architecture Review And Developer Console Foundation

- Audited the architecture against object/capability/endpoint/message/ring/grant
  invariants.
- Kept hardware access limited to approved backend files.
- Added framebuffer text console behavior for non-serial local output.
- Added PS/2 keyboard input and TTY/input endpoint plumbing.
- Added ScadekOS boot-script command execution and a native command runner.
- Validated `version`, `ls`, `cat`, `run`, `rings`, `grants`, `services`, and `caps`.

### M31: Session Namespace And Directory-Aware VFS

M31 completed the bridge from a kernel-capability bootstrap into a ScadekOS
user-visible session namespace.

M31 scope:

- 新增 boot/session namespace endpoint。
- `/init` 通过一个 bootstrap cap 查询 `console`、`tty`、`vfs`、`proc`。
- `proc` spawn 支持把 session cap 或指定 bootstrap cap 传给子进程。
- 用户 IPC 放行对应 namespace/VFS 消息。
- VFS/tmpfs 增加 `STAT` 和 `LISTDIR`。
- 保持 TTY 仍是 polling + yield，不做复杂阻塞/waitset。
- 加 boot self-test 和 ScadekOS smoke marker。

M31 validation coverage:

- Session service registration and lookup.
- Session-mediated lookup of console, TTY, VFS, proc, and grant-test services.
- VFS `STAT` for `/`.
- VFS `LISTDIR` for `/` and `/bin`.
- `/init` to `/runner` bootstrap through proc spawn.
- `run /bin/hello` with an explicit bootstrap cap.
- `rings` and `grants` user demos through the command runner.
- Keyboard smoke path: QEMU `sendkey help` reaches PS/2 input, TTY polling, and
  ScadekOS command dispatch.
- No-serial framebuffer smoke path: local framebuffer output remains nonblank.

## Current Validation Matrix

SCDK validation:

- `tools/arch-check.sh`
- `tools/audit-console-access.sh`
- boot-time self-tests ending in `[test] all core tests passed`

ScadekOS validation:

- `make smoke`
- `make smoke-no-serial`
- `make smoke-keyboard`

Expected release markers:

- `SCDK: v0.4.0-alpha.3`
- `ScadekOS: v0.1.0-devpreview.3`
- `milestone=kernel-os-parallel`
- `[boot] milestone 31 complete`
- `[scadekos] interactive console ready`

## Open Areas After M31

The baseline is still a prototype. Remaining areas are tracked after the M0-M31
foundation rather than as part of it:

- Service migration out of kernel space where useful.
- A richer async model beyond polling + yield.
- Waitsets or blocking receive primitives.
- Stronger capability derivation and revoke trees.
- Persistent filesystems and block devices.
- PCI, DMA, IOMMU, network, and SMP.
- ELF loading beyond the current flat payload model.
- A compatibility layer above native SCDK interfaces, if needed.
