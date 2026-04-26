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

## Current Milestone

This tree currently implements Milestone 0 through Milestone 7:

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

PMM, VMM, scheduler, filesystem, and user-space support are intentionally not
implemented yet.

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
[boot] milestone 7 complete
```

## VMware

Create a new 64-bit x86 virtual machine and attach `build/scdk.iso` as the
virtual CD/DVD image. Enable a serial port if you want to capture COM1 logs.

## License

SCDK is licensed under the Mozilla Public License 2.0. See `LICENSE`.
