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

Current version: `0.2.0-alpha.2`

## Current Milestone

This tree currently implements Milestone 0 through Milestone 17:

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

Filesystem and user-space support are intentionally not implemented yet.

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

Expected serial output includes:

```text
[boot] SCDK kernel entered
[boot] serial ok
[boot] framebuffer ok
[boot] memory map received
[boot] capability core initialized
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
[test] all core tests passed
[boot] milestone 17 complete
```

## VMware

Create a new 64-bit x86 virtual machine and attach `build/scdk.iso` as the
virtual CD/DVD image. Enable a serial port if you want to capture COM1 logs.

## License

SCDK is licensed under the Mozilla Public License 2.0. See `LICENSE`.
