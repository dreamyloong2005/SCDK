# SCDK

SCDK is an experimental x86_64 kernel architecture.

It explores a hybrid design:

- monolithic-kernel-like fast paths
- microkernel-like explicit boundaries
- capability-based object access
- per-core sharding
- ring-based asynchronous data paths
- zero-copy grants
- queue-level device authorization
- migratable services and kernel capsules

SCDK is not a UNIX clone. POSIX compatibility may be added later as a
user-space compatibility layer, but it is not the native kernel API.

## Version

Current version: `0.4.0-alpha.2`

## Current Milestone

This tree currently implements Milestone 0 through Milestone 30:

- Limine bootable ISO
- higher-half x86_64 freestanding kernel ELF
- COM1 serial logging
- Limine framebuffer test pattern
- panic path
- info/warn/error serial logging
- panic register dump placeholder
- minimal memory/string routines
- static object manager
- static capability table
- boot-time object/capability self-tests
- static endpoint table
- bootstrap service registry
- message envelope and endpoint dispatch
- kernel-resident console service endpoint
- boot-time endpoint/message self-tests
- static ring storage
- submission and completion queues
- boot-time ring producer/consumer self-tests
- static grant storage
- grant rights and bounds checks
- boot-time grant read/write-permission self-tests
- console service as the first SCDK service
- normal post-console logs routed through endpoint/message dispatch
- Limine memory map based PMM
- static physical 4 KiB page stack
- boot-time page alloc/free and reserved-overlap self-tests
- current CR3 page-table inspection
- bootstrap x86_64 VMM map/unmap helpers
- boot-time mapped-page read/write self-test
- page fault logging placeholder
- task object skeleton
- thread object skeleton
- current task/thread scheduler placeholder
- cooperative yield stub self-test
- unified core regression self-test runner
- interface status definitions split into a dedicated status header
- page-backed kernel heap
- kernel allocation, zeroed allocation, free-list reuse, and typed object storage tests
- capability-protected address-space objects
- address-space user mapping, activation, range rejection, and unmap tests
- cooperative kernel-thread scheduler with per-core run queue
- x86_64 context switch path for kernel threads
- built-in ring 3 user-mode entry prototype
- minimal debug and exit syscall path
- minimal syscall dispatch for debug-write, endpoint-call, yield, and exit
- user-to-kernel endpoint call path with user message validation
- user-mode console write routed through syscall, endpoint, message, and service
- minimal user task lifecycle with address-space and main-thread objects
- user task run, exit, and cleanup self-tests
- Limine initrd module loading
- small ustar-backed initrd parser with list/find self-tests
- initrd-backed tmpfs service endpoint
- tmpfs open/read/close message path with file capabilities
- tmpfs write-not-supported self-test
- SCDK-native VFS service mounted over tmpfs at /
- VFS open/read/close routing through endpoint/message
- flat `/init` and `/hello` user executables built into the initrd
- minimal loader that reads flat user executables through VFS
- loader-created user task, address space, and main thread
- `/hello` console write routed through syscall, endpoint, message, and service
- kernel-resident process manager service endpoint
- `/init` spawn request routed through syscall, endpoint, proc service, VFS, and loader
- user-visible grant create/revoke syscalls
- user grants bound to source task, target endpoint, range, rights, and lifetime
- grant-test service validates read access, write denial, bounds rejection, and revoke
- user-visible ring create/bind/submit/poll syscalls
- user rings bound to endpoint capabilities and owned by the source task
- console service consumes ring descriptors and reads payloads through grants
- ring-test user executable submits and polls a batch of 16 completions
- console frontend split from the serial/framebuffer backend
- normal console output routed through endpoint/message or ring/grant frontend paths
- direct serial/framebuffer writes audited and isolated to early boot, panic, arch drivers, and console backend
- capability revocation with stale-generation rejection
- revoked capabilities rejected by future lookup and rights checks
- ring endpoint bindings reject revoked target endpoint capabilities
- user grant endpoint bindings reject revoked target endpoint capabilities
- minimal x86_64 #PF/#GP/#UD fault gates
- user page faults kill only the offending task
- invalid syscall and bad user pointer paths route through fault handling
- kernel faults still panic
- single-core PIT timer interrupt path
- timer tick counter and IRQ0 handling through the x86_64 IDT
- minimal timer-driven preemption over kernel threads
- kernel-resident device manager service endpoint
- fake device and device-queue objects protected by capabilities
- queue bind authorization through device-queue capabilities
- unauthorized and revoked queue capability rejection tests
- M30 architecture review documentation and invariant checklist
- repeatable source-level architecture check script
- boot-time architecture review serial markers
- early console wrapper for COM1 boot/panic output
- framebuffer text console backend with clear, cursor, backspace, tab, newline, and scroll handling
- console backend router with serial mirror and framebuffer text output
- SCDK-native console messages for grant-backed write, clear, and info
- PS/2 keyboard polling backend with printable US ASCII, Enter, Backspace, Tab, and Shift handling
- TTY/input service endpoint with input event polling
- console/TTY self-tests for grant write and input event paths
- direct console/input hardware access audit script

Full filesystem and full user-space support are intentionally not implemented yet.

## Build

Load the local development tools into `PATH`:

```sh
. .devtools/env.sh
```

Build the ISO:

```sh
make iso
```

The output is:

```text
build/scdk.iso
```

## Run With QEMU

```sh
tools/run-qemu.sh
```

Expected serial output, or framebuffer text output when serial is absent,
includes:

```text
[boot] SCDK kernel entered
[console] serial debug sink enabled
SCDK framebuffer boot console ready
[boot] framebuffer ok
[boot] memory map received
[boot] capability core initialized
[console] backend init ok
[console] framebuffer text backend ok
[console] serial mirror backend ok
[console] frontend ready
[console] service endpoint registered
[console] backend ready
[console] direct hardware access audit pass
[input] ps2 keyboard backend ok
[tty] service endpoint registered
[tty] input event path pass
[boot] endpoint/message core initialized
[boot] console service initialized
[boot] ring core initialized
[boot] grant core initialized
[boot] pmm initialized
[boot] vmm initialized
[heap] init ok
[heap] alloc/free smoke test pass
[heap] typed allocation smoke test pass
[aspace] create pass
[aspace] user map pass
[aspace] kernel range reject pass
[aspace] unmap pass
[sched] thread A step 1
[sched] thread B step 1
[sched] thread A step 2
[sched] thread B step 2
[boot] scheduler initialized
[user] preparing test address space
[user] entering ring3
[syscall] debug call from user mode
[user] endpoint call console write pass
[user] returned or exited
[task] user task created
[task] main thread started
[task] user task exited
[task] cleanup pass
[initrd] found module
[initrd] file: /init
[initrd] file: /hello
[initrd] file: /grant-test
[initrd] file: /ring-test
[initrd] file: /etc/scdk.conf
[initrd] file: /hello.txt
[initrd] list pass
[tmpfs] service started
[tmpfs] open /hello.txt pass
[tmpfs] read /hello.txt pass
[vfs] service started
[vfs] mount tmpfs at / pass
[vfs] open /hello.txt pass
[vfs] read /hello.txt pass
[loader] loading /hello
[loader] loaded /hello
[loader] mapped /hello
[user] hello from spawned process
[loader] /hello start pass
[proc] service started
[loader] loading /init
[loader] loaded /init
[loader] mapped /init
[proc] spawn /hello
[loader] loaded /hello
[user] hello from spawned process
[proc] process exited
[loader] /init start pass
[loader] loading /grant-test
[grant] user read grant pass
[grant] write denied pass
[grant] bounds reject pass
[grant] revoke pass
[console] grant write path pass
[loader] loading /ring-test
[ring] user ring create pass
[ring] submit batch 16
[ring] completion batch 16 pass
[revoke] cap revoke pass
[revoke] stale generation reject pass
[revoke] ring binding reject pass
[revoke] grant binding reject pass
[fault] user page fault
[fault] task killed
[sched] continuing
[fault] invalid syscall handled
[fault] bad user pointer handled
[timer] init ok
[timer] tick ok
[sched] preempt thread A
[sched] preempt thread B
[devmgr] service started
[devmgr] fake device registered
[devmgr] queue capability created
[devmgr] queue bind pass
[devmgr] unauthorized queue bind reject pass
[m30] architecture review start
[m30] capability boundary review pass
[m30] hardware access review pass
[m30] user pointer review pass
[m30] ring/grant review pass
[m30] architecture review complete
[test] all core tests passed
[boot] milestone 30 complete
```

COM1 serial is an optional debug sink. The ISO is expected to boot with
`-serial none` and still show visible framebuffer text output.

## VMware

Create a new 64-bit x86 virtual machine and attach `build/scdk.iso` as the
virtual CD/DVD image. Enable a serial port if you want to capture COM1 logs.

## License

SCDK is licensed under the Mozilla Public License 2.0. See `LICENSE`.
