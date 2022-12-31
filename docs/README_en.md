<p align="center">
  <img width="auto" src="img/lunaix-os-logo.png">
</p>

<p align="center">
  <a href="../README.md"><b>简体中文</b></a> | <span><b>English</b></span>
</p>

# The LunaixOS Project

LunaixOS - A simple (yet naive), POSIX-complaint (hopefully!), operating system from scratch. This is started for educational & learning purpose, for my online video tutorial on OS development **in Chinese**：[*Do It Yourself an Operating System*](https://space.bilibili.com/12995787/channel/collectiondetail?sid=196337).

## Features

This operating system is a macro-kernel, has its root in Intel's x86 platform and its ecosystem. It runs in protected mode and uses 4GiB addressing with two-level paging mechanism. It does not have x86_64 variant and does not support multi-core machine.

The virtual address space is divided into two parts, that is, 3GiB for user space (0x400000 ~ 0xBFFFFFFF) and 1GiB for kernel space (0xC0000000 ~ 0xFFFFFFFF). Such paradigm is a common practicing found in major operating systems, for example x86_32 version of Linux and Windows. For a more detailed arrangement of memory in LunaixOS, please refer to [LunaixOS Virtual Memory Mappings](img/lunaix-os-mem.png).

The following list presents all features it does have in current stage.

+ Multiboot for bootstrapping
+ APIC/IOAPIC as interrupt controller and programmable timer.
+ ACPI table parsing.
+ Virtual memory & paging
+ Memory management & demand paging
+ PS/2 Keyboard support
+ Muti-tasking and task management
+ 50 commonly used POSIX syscall（[See Appendix 1](#appendix1)）
+ User Space
+ Signal
+ PCI 3.0
+ PCIe 1.1 (WIP)
+ Serial ATA AHCI
+ File System
  + Virtual File System
  + ISO9660
    + Original
    + Rock Ridge Extension (WIP)
+ GDB Remote debugger (via UART)

The OS has been tested in the following environments, including both virtual and bare-metal.

+ QEMU (>=7.0.0)
+ Bochs (Not really, as it lacks support on SATA)
+ Virtualbox
+ Dell G3 3775 Laptop

## Project Structure

| | |
|-----|------|
| [lunaix-os](../lunaix-os/) | LunaixOS source code |
| [slides](../slides/) | Slides used in my videos |
| [reference-material](../reference-material/)| References |

## Compile and Build

You will need following dependencies in order to build LunaixOS

+ gcc **(target=i686-elf)**
+ binutils
+ make
+ xorriso
+ grub-mkrescue

The following `make` actions are available to use.

| Action | Description |
|---|---|
| `make all` | Build bootable（`-O2`） |
| `make all-debug` | Build debuggable bootable（`-Og`） |
| `make run` | Boot the OS with QEMU |
| `make debug-qemu` | Build and debug with QEMU |
| `make debug-bochs` | Build and debug with Bochs |
| `make debug-qemu-vscode` | Used for integrated with vscode for better debug experience |
| `make clean` | Delete recent build |

When a bootable is built, it is an `.iso` image file and can be found under `build/` directory.

## Running and Issue

To run LunaixOS, you will be required to attach a usable virtual disk image. You can use the following handy command to create one:

```bash
qemu-img create -f vdi machine/disk0.vdi 128M
```

However, if you have image other than standard `.vdi`, you shall change `configs/make-debug-tool` accordingly.

Locate this particular line:

```
-drive id=disk,file="machine/disk0.vdi",if=none \
```

and replace the default path with your preferred.

There are also many other ways to create disk image, such as the aforementioned `qemu-img`.

The next thing is about issue. In the most common situation, the master branch should be stable in aforementioned running environments. However, one might encounter compiler-optimization related issue when they compiled with `-O2`. Such condition will usually be addressed and fixed in the following commits. Should the issue remains, please post your issue here.

To maximize the value of this section, we will provide some FAQ below that hopefully resolve some "not working" complains:

#### Q1: Prompting "i8042: not found" after boot in QEMU

This is a issue related to misconfiguration of ACPI table in QEMU, and has been addressed in version 7.0.0.

#### Q2: General Protection exception get triggered

It is possible a race condition result from multiprogramming. This is not possible in current stage, and we however encourage you to report in case of it.

#### Q3: Prompting "AHCI: Not found." after boot in Bochs

This is an expected behaviour, as Bochs does not support SATA!

## Referenced Tutorial

**THERE IS NONE !** The project is based solely on my understanding on operating system concepts and relevant research. And has referenced no third party tutorial nor open source project like the Linux or other hobbyist project.

Thus, the author has devoted large amount of time to go through any related materials such as technical references, manuals, textbooks, and industrial standards. To further ensure the technology used and knowledge taught is up-to-date and is coming "straight from the factory".

You can find most of aforementioned materials in [reference-material](../reference-material/).

The following list also enumerated such materials the author has used:

#### Manuals, Technical References and Standards

+ [Intel 64 and IA-32 Architecture Software Developer's Manual (Full Volume Bundle)](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
+ [ACPI Specification (version 6.4)](https://uefi.org/sites/default/files/resources/ACPI_Spec_6_4_Jan22.pdf)
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
+ [ECMA-119 (ISO9660)](https://www.ecma-international.org/publications-and-standards/standards/ecma-119/)
+ Rock Ridge Interchange Protocol (RRIP: IEEE P1282)
+ System Use Sharing Protocol (SUSP: IEEE P1281)

**DISCLAIMER: All rights of PCI-related specification is reserved by PCI-SIG. It is provided ONLY for learning purpose. Any commercial use should purchase a copy from PCI-SIG**

#### Textbook

+ *Computer System - A Programmer's Perspective Third Edition* (Bryant, R & O'Hallaron, D), a.k.a. CS:APP
+ *Modern Operating System* (Tanenbaum, A)

#### Website

+ [OSDev](https://wiki.osdev.org/Main_Page) - For material gathering.
+ [FreeVGA](http://www.osdever.net/FreeVGA/home.htm) - For VGA references.
+ GNU CC & LD online documentation.
+ [PCI Lookup](https://www.pcilookup.com/) - For device look up

#### Others

+ Linux Manual - For learning the system call behavior on real machine.

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
2. `mkdir(2)`※
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
2. `execve(2)`※

**LunaixOS**

1. `yield`
2. `geterrno`
3. `realpathat`

( **※**：Indicate syscall is not tested )

## Appendix 2: Debugging with GDB remotely via UART

The LunaixOS kernel comes with a built-in GDB debugging server, which runs on COM1@9600Bd. However, LunaixOS must be in debug mode before involving GDB.

One could trigger the debug mode by writing a byte sequence `0x40` `0x63` `0x6D` `0x63`, to the same serial port. A text "DEBUG MODE" with magenta-coloured background shall be present at the bottom of the screen.

Note that, whenever the text appears, the LunaixOS always halt all activities other than the debugging server, which means no scheduling and no external interrupt servicing. Users are now recommended to attach their GDB and drive the kernel with the debugging workflow.

Currently, LunaixOS implements the required minimal server-side command subset required by GDB Remote Protocol, namely, `g`, `G`, `p`, `P`, `Q`, `S`, `k`, `?`, `m`, `M`, `X`. Which should be enough to cover most debugging activities.

When debugging is finished, one shall disconnect with `kill` command. This command will not force LunaixOS to power down the computer, instead it just resume the execution (identical behavior as `c` command). However, disconnecting does not means exiting of debug mode. The debug mode is still actived and any subsequent GDB attaching request shall remain the highest priority amongst all other activity. One shall deactivate the debug mode by writing byte sequence `0x40` `0x79` `0x61` `0x79` to the port, after GDB detached.

### Limitations

Currently, one should avoid the use of `info stack`, `bt` or any other command that involves stack unwinding or stack backtracing. As it will somehow corrupt the stack layout and result in undefined behaviour. This issue should be addressed in future releases.
