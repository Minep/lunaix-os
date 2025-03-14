<p align="center">
  <img width="auto" src="docs/img/lunaix-os-logo.png">
</p>

<p align="center">
  <a href="docs/README.cn.md">简体中文</a> | <a href="#the-lunaixos-project">English</a>
</p>

# The Lunaix Project

The Lunaix, LunaixOS, or Lunaix kernel to be exact, is a multi-architectural general purpose kernel written from scratch by the author *independently*. It is the author's years-long personal endeavor and also a challenge to oneself for writing a functioning kernel **without** any external reference such as existing implementations, tutorials and books (with code) on kernel design. As a result, this project is a manifestation of the author's own research and understanding on the board topic of operating system design and implementation.

## Introduction

The overall design of lunaix is aimed to be modern and POSIX-compliance. In a nutshell, Lunaix is:

+ fully-preemptive
+ modular design with configurable components at compile-time and extendable subsystems
+ high-performance by utilising advanced caching techniques and infrastructure.
+ fault-tolerance with sophisticated builtin error handling and tracing techniques.
+ robust in nature with techniques such as proactive deadlock detection and driver isolation mechanism.

The author has put a significant amount of time on devising better abstractions, advance kernel features and various optimisation techniques. To give a better understanding (and appreciation) of the works being done, the following non-exhaust list has been compiled with features that are currently supported in lunaix:

+ Multi-ISA
  + x86_32
  + x86_64
  + Aarch64 (W.I.P)
+ Boot protocol
  + abstraction for different protocol
  + configurable kernel boot-time parameters
+ Platform resource management and definition
  + ACPI
  + Devicetree
+ Memory management
  + architecture-neutral abstraction
  + on-demand paging
  + copy-on-write and page sharing
  + compound page support
  + explicit huge page support (sorry, not THP!)
  + reverse mapping indexing (rmap)
  + memory compaction (W.I.P)
  + slab-alike object allocator
  + highmem support
  + remote address space accessing
+ Multi-tasking
  + Protection level and process image isolation
  + Native threading support (no more lightweight process nonsense)
  + Signal mechanism
  + Kernel level multi-tasking (i.e. kernel threads)
  + Round-robin scheduling (for now)
  + Preemptive kernel design
  + taskfs: file system interface to process and threads
+ File system
  + virtual file system framework
  + ...with POSIX compliant interfaces
  + file system mounting mechanism
  + page cache for file IO
  + node cache for vfs structure representation.
  + ext2 (rev.0, rev.1)
  + iso9660 (rock-ridge)
  + twifs: file system interface to kernel states.
+ Device management and interrupt handling
  + architecture-neutral design
  + generalised driver framework
  + generalised irq framework
  + driver modularisation design
  + support asynchronous device model
  + devfs: file system interface to device subsystem
+ Block I/O (blkio)
  + generalised block IO interface and encapsulation
  + blkio packets caching
  + asynchronous blkio operation in nature
+ Serial I/O
  + POSIX-compliant serial port model
  + serial device driver framework (part of driver framework)
+ Caching Infrastructure
  + primitive: generic sparse associative array (spatial data)
  + LRU replacement policy and pooling
  + kernel daemon for scheduled cache eviction
+ Error handling and detection
  + stack back-tracing with symbol resolution
  + nested exception unfolding
  + CPU state dumping
  + Deadlock/hung-up detection

For the device drivers that are currently support see below:

+ Arhcitecture Neutral
  + UART 16650-compatible driver
  + Serial ATA AHCI
  + PCI 3.0
  + PCIe 1.1
  + Standard VGA
+ Intel x86
  + RTC (Intel PCH)
  + IOAPIC irq controller
  + APIC Timer
  + Legacy i8042 keyboard controller
+ ARM
  + GICv3
  + PL011 (W.I.P)

By the way, do you know there is an online video course  by the author on the design of lunaix? [Check it out](https://space.bilibili.com/12995787/channel/collectiondetail?sid=196337) (although it is in Chinese!) 

## Project Structure

| Path | Description |
|-----|------|
| [lunaix-os](../lunaix-os/) | LunaixOS source code |
| [slides](../slides/) | Slides used in my videos |
| [reference-material](../reference-material/)| References |

## Compile and Build

Building lunaix is simple, no more bloated dependencies to install, basic `build-essentials` installation plus a python are sufficient.

+ gcc (recommend v12+)
+ binutils
+ make
+ python (recommend v3.11+)

And also one should have environment variable `ARCH=<arch>` exported, where `<arch>` is one of the supported arhcitecture (`x86_32`, `x86_64`, `aarch64`).

For cross compilation, also export `CX_PREFIX` to the gcc prefix for the corresponding `<arch>`.

The following `make` actions are then available to use.

| Make command | Usage |
| ---- | ---- |
| `make all`               | Build the kernel bin |
| `make rootfs`            | Build the stock rootfs |
| `make clean`             | clean |
| `make config`            | menuconfig |

A successful build will give `build/bin/kernel.bin`.

**Please note: this is the kernel, not a bootable image, it require a bootloader to boot and specify the rootfs.**

## Booting the kernel

Since lunaix is a kernel, much like linux. It requires additional setup to do the magic. And, as in "much like linux", methods to make linux kernel boot can also apply to lunaix without or with little translation, as we will discuss below.

The bootloader part is generic, any bootloader, for example GRUB will work (not tested for UEFI, but I expect this would be an exception), or booting up in QEMU using `-kernel` option

The kernel command line, is however, a bit differentiated.
The syntax is similar, both takes form of space-separated array of `<key>=<val>` pairs or boolean `<flag>`.

Currently, lunaix support the following options

| Option | Default Value | Optional | Usage |
| ------ | ----          |  -----   | ----  |
| console | `/dev/ttyS0`   | No | Specify the system console device, path within lunaix's devfs |
| rootfs | `/dev/block/sda` | No | Specify the device contain rootfs image, path within lunaix's devfs |
| init | `/init` | Yes | Path within rootfs of the `init` |

### A quick 'Get Started'

One can use the `live_debug.sh` provided in the lunaix root directory to quickly bring up the system with default parameters (also used by the author for debugging).

Following the steps:

1. Select an architecture `<arch>`
2. Check the compilation prerequisites and presence of `qemu-system-<arch>`
3. Run `make ARCH=<arch> user` to build the stock user program
4. Run `make ARCH=<arch> rootfs` to build stock rootfs image, require support of `dd`，`mkfs.ext2`, `mount -o loop`, `mktemp`.
5. Run `ARCH=<arch> live_debug.sh` to boot in QEMU with gdb hooked (one should see a gdb session)
6. telenet to `localhost:12345`, this is QEMU emulated serial port
7. type `c` in the active gdb session and commence emulation.
8. Congrats, enjoy your lunaix!
(or submit an issue)


## Submit an Issue

If one ran into bug, one can submit an issue by filling up the following template

```
1. Describe the problem
    "How does it look like, anything descriptive: visual, sonic, emotional experience"
2. Steps to reproduce
    "How you ran into this mess?"
3. Expected behaviour
    "What do you intended/expected to achieve/to be"
4. Lunaix's panic trace (if applicable)
5. Other clues that you think might be helpful
```


## Limitations

The development process is still in motion, any limitation can be categorised as a feature yet to be. However, some features that the author considered to be the most urgent and wish the matters to be discussed.

Lunaix is under impression of uniprocessor and not capable of running in SMP environment. This is major held back of being a modern operating system. It has the highest priority among all other tasks

Lunaix do not have a mature (or even, an infant) user space ecosystem, mainly because the lack of a proper and sophisticated libc. Efforts need to be done for porting one to the target. However, given the author's tight schedule, this task is unfortunately still beyond the horizon.

## Acknowledgement

Albeit one must realise that the author has mentioned it in the very beginning, the author would like to emphaise **again** on the nature of this project.

As a personal challenge, this project is independently developed by the author single-handly, which means:

+ No reference to existing tutorials, books, online courses or any open source project that might provide any example, hint or working prototype on the design and implementation of kernel, subsystems or anythings that can be contributed towards a working prototype.
+ The author has no prior knowledge on Linux kernel through out 90% of the project time.
+ All knowledge on the kernel design is coming from the basic textbook on operating system theory, that is, *Modern Operating System* by Tanenbaum.
+ All knowledge on the system programming is coming from the basic textbook, that is, *Computer System - A Programmer's Perspective Third Edition*
+ All knowledge on the generic framework design and driver development are ingested from various technical specifications gathered across the Internet.

## References

+ Intel 64 and IA-32 Architecture Software Developer's Manual (Full Volume Bundle)
+ ACPI Specification (version 6.4)
+ Devicetree Specification
+ ARM® Generic Interrupt Controller (v3)
+ Arm® Architecture Reference Manual (Profile-A)
+ Procedure Call Standard for the Arm® 64-bit Architecture (AArch64)
+ IBM PC/AT Technical Reference
+ IBM VGA/XGA Technical Reference
+ 82093AA I/O Advanced Programmable Controller (IOAPIC) (Datasheet)
+ MC146818A (Datasheet)
+ Intel 500 Series Chipset Family Platform Controller Hub (Datasheet - Volume 2)
+ PCI Local Bus Specification, Revision 3.0
+ PCI Express Base Specification, Revision 1.1
+ PCI Firmware Specification, Revision 3.0
+ Serial ATA - Advanced Host Controller Interface (AHCI), Revision 1.3.1
+ Serial ATA: High Speed Serialized AT Attachment, Revision 3.2
+ SCSI Command Reference Manual
+ ATA/ATAPI Command Set - 3 (ACS-3)
+ ECMA-119 (ISO9660)
+ Rock Ridge Interchange Protocol (RRIP: IEEE P1282)
+ System Use Sharing Protocol (SUSP: IEEE P1281)
+ Tool Interface Standard (TIS) Portable Formats Specification (Version 1.1)
+ *Computer System - A Programmer's Perspective Third Edition* (Bryant, R & O'Hallaron, D), a.k.a. CS:APP
+ *Modern Operating System* (Tanenbaum, A)
+ Free VGA, http://www.osdever.net/FreeVGA/home.htm 
+ GNU CC & LD online documentation.
+ PCI Lookup, https://www.pcilookup.com/
+ Linux man pages

## Appendix 1: Supported System Call<a id="appendix1"></a>

**Unix/Linux/POSIX**

1. `sleep(3)`
1. `wait(2)`
1. `waitpid(2)`
1. `fork(2)`
1. `getpid(2)`
1. `getppid(2)`
1. `getpgid(2)`
1. `setpgid(2)`
1. `brk(2)`
1. `sbrk(2)`
1. `_exit(2)`
1. `sigreturn(2)`
1. `sigprocmask(2)`
1. `signal(2)`
1. `kill(2)`
1. `sigpending(2)`
1. `sigsuspend(2)`
2. `read(2)`
2. `write(2)`
2. `open(2)`
2. `close(2)`
2. `mkdir(2)`
2. `lseek(2)`
2. `readdir(2)`
2. `readlink(2)`※
2. `readlinkat(2)`※
2. `rmdir(2)`※
2. `unlink(2)`※
2. `unlinkat(2)`※
2. `link(2)`※
2. `fsync(2)`※
2. `dup(2)`
2. `dup2(2)`
2. `symlink(2)`※
2. `chdir(2)`
2. `fchdir(2)`
2. `getcwd(2)`
2. `rename(2)`※
2. `mount(2)`
2. `unmount` (a.k.a `umount(2)`)※
2. `getxattr(2)`※
2. `setxattr(2)`※
2. `fgetxattr(2)`※
2. `fsetxattr(2)`※
2. `ioctl(2)`
2. `getpgid(2)`
2. `setpgid(2)`
2. `mmap(2)`
2. `munmap(2)`
2. `execve(2)`
3. `poll(2)` (via `pollctl`)
3. `epoll_create(2)` (via `pollctl`)
3. `epoll_ctl(2)` (via `pollctl`)
3. `epoll_wait(2)` (via `pollctl`)
4. `pthread_create`
4. `pthread_self`
4. `pthread_exit`
4. `pthread_join`
4. `pthread_kill`
4. `pthread_detach`
4. `pthread_sigmask`

**LunaixOS**

1. `yield`
2. `geterrno`
3. `realpathat`

( **※**：Indicate syscall is not tested )

## Appendix 2: Debugging with GDB remotely via UART

**(((( Not working yet, need rework ))))**

The LunaixOS kernel comes with a built-in GDB debugging server, which runs on COM1@9600Bd. However, LunaixOS must be in debug mode before involving GDB.

One could trigger the debug mode by writing a byte sequence `0x40` `0x63` `0x6D` `0x63`, to the same serial port. A text "DEBUG MODE" with magenta-coloured background shall be present at the bottom of the screen.

Note that, whenever the text appears, the LunaixOS always halt all activities other than the debugging server, which means no scheduling and no external interrupt servicing. Users are now recommended to attach their GDB and drive the kernel with the debugging workflow.

Currently, LunaixOS implements the required minimal server-side command subset required by GDB Remote Protocol, namely, `g`, `G`, `p`, `P`, `Q`, `S`, `k`, `?`, `m`, `M`, `X`. Which should be enough to cover most debugging activities.

When debugging is finished, one shall disconnect with `kill` command. This command will not force LunaixOS to power down the computer, instead it just resume the execution (identical behavior as `c` command). However, disconnecting does not means exiting of debug mode. The debug mode is still actived and any subsequent GDB attaching request shall remain the highest priority amongst all other activity. One shall deactivate the debug mode by writing byte sequence `0x40` `0x79` `0x61` `0x79` to the port, after GDB detached.

### Limitations

Currently, one should avoid the use of `info stack`, `bt` or any other command that involves stack unwinding or stack backtracing. As it will somehow corrupt the stack layout and result in undefined behaviour. This issue should be addressed in future releases.
