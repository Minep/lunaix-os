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

The memory is split into two parts, that is, 3GiB for user space (0x400000 ~ 0xBFFFFFFF) and 1GiB for kernel space (0xC0000000 ~ 0xFFFFFFFF). Such paradigm is a common practicing found in major operating systems, for example x86_32 version of Linux and Windows. For a more detailed arrangement of memory in LunaixOS, please refer to [LunaixOS Virtual Memory Mappings](docs/img/lunaix-os-mem.png).


The following list presents all features it does have in current stage.

+ Multiboot for bootstrapping
+ APIC/IOAPIC as interrupt controller and programmable timer.
+ ACPI table parsing.
+ Virtual memory & paging
+ Memory management & demand paging
+ PS/2 Keyboard support
+ Muti-tasking and task management
+ 15 commonly used POSIX syscall（[See Appendix 1](#appendix1)）
+ User Space
+ Signal

## Project Structure

| | |
|-----|------|
| [lunaix-os](lunaix-os/) | LunaixOS source code |
| [slides](slides/) | Slides used in my videos |
| [reference-material](reference-material/)| References |

## Referenced Tutorial

**THERE IS NONE !** The project is based solely on my understanding on operating system concepts and relevant research. And has referenced no third party tutorial nor open source project like the Linux or other hobbyist project.

Thus, the author has devoted large amount of time to go through any related materials such as technical references, manuals, textbooks, and industrial standards. To further ensure the technology used and knowledge taught is up-to-date and is coming "straight from the factory".

You can find most of aforementioned materials in [reference-material](reference-material/).

The following list also enumerated such materials the author has used:

#### Manuals, Technical References and Standards
+ [Intel 64 and IA-32 Architecture Software Developer's Manual (Full Volume Bundle)](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
+ [ACPI Specification (version 6.4)](https://uefi.org/sites/default/files/resources/ACPI_Spec_6_4_Jan22.pdf)
+ IBM PC/AT Technical Reference
+ IBM VGA/XGA Technical Reference
+ 82093AA I/O Advanced Programmable Controller (IOAPIC) (Datasheet)
+ MC146818A (Datasheet)
+ Intel 500 Series Chipset Family Platform Controller Hub (Datasheet - Volume 2)

#### Textbook
+ *Computer System - A Programmer's Perspective Third Edition* (Bryant, R & O'Hallaron, D), a.k.a. CS:APP
+ *Modern Operating System* (Tanenbaum, A)


#### Website 
+ [OSDev](https://wiki.osdev.org/Main_Page) - For material gathering.
+ [FreeVGA](http://www.osdever.net/FreeVGA/home.htm) - For VGA references.
+ GNU CC & LD online documentation.

#### Others
+ Linux Manual - For learning the system call behavior on real machine.


## Appendix 1: Supported System Call<a id="appendix1"></a>

### Unix/Linux/POSIX
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

### Unique to LunaixOS

1. `yield`
