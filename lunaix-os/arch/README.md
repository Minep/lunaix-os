# Porting Lunaix to Other ISAs

This document briefly describe how to add support for other ISA

## Introduction

As of most modern operating system, Lunaix's abstraction layer allow
a clean separation between regular kernel code and the
architecture specific code, these parts defined as follow:
  + kernel code: Provide the functionality of a kernel that do not assume 
    anything other than the kernel basic service and feature. This part of
    code is thus said to be architecture-agnostic.
  + architecture specific code: Provide implementation of low-level operations
    or features required by the aforementioned kernel kernel basic service and 
    feature for them to function normally and expected.

## General Structure

Each architectural support is housed under the directory of 
their respective name. For example, all support for x86 is
placed under `x86/`. 

Aside from this, a special architecture
called `generic` is defined, which designed to be a fallback
option for any features/operations that are not being implemented or does not required to be implemented in architecture-specific way. However, not all feature/operations
have their respective fallback; In that case, presence of architectural support for these features/operations are mandatory.

Regardless, a typical architecture support must be a valid derivation of the following subdirectory layout:

```
<arch>/
    includes/
        asm/
        linking/
        sys/ 
    ...
    (anything you want)
```

where:

  + `asm` define all assembly level headers. These header is
    typically designated to perform low-level architectural
    tasks such as register manipulation, MMU/TLB
    maintainance, SoC resource managment, or context saving/restoring.
  + `sys` define all headers that is related to some kernel 
    service. These services, similar but unlike the **regular kernel code**, require architectural support or 
    optimisation. For example, syscall interfacing or cpu state dumping for debugging purpose.
  + `linking` contain linker scripts that describe the linking
    procedure for linking the architectural support with other subsystem such that a valid kernel binary can be produced. There is no limit on how many linker script can be put under it, however, only one specific linker script will be taken into account when linking, more detail as follow.

An implementation is required to follow this structure and must implement all mandatory header file, as described in the following section.


## Implementation

### Mandatory and Optional Features

Lunaix provide bunch of headers that **MUST** be implemented in order to behave correctly.

```
includes/asm/cpu.h
includes/asm-generic/isrm.h
includes/asm/muldiv64.h
includes/asm/hart.h
includes/asm/mempart.h
includes/asm/abi.h
includes/asm/tlb.h
includes/asm/pagetable.h

includes/sys/syscall_utils.h
includes/sys/failsafe.h
includes/sys/gdbstub.h
includes/sys-generic/elf.h
```

An implementation should copy these file out of `arch/x86`, replacing any `x86` specific function-body/structure-members/macros with the chosen architecture.

Most headers located under `generic/` has default implementation and is designed to eliminate common code. The defaults can be optionally override, except for those with `fail()` as their only definition or those listed in above.

### Preparing Linker Script

Lunaix use CC's preprocessing system on creating modularised linker script. Such linker scripts in denoted with 'ldx' as extension in contrast to the genuine one. 

There is no limit on the number of linker scripts present. However, the following list the mandatory linker scripts required for an architectural support, as these scripts are referenced by the main script.

+ `base_defs.ld.inc` Define the macros related to kernel 
  binary structure, the following macros are mandatory:
    + `LOAD_OFF` base physical address of kernel when loaded into memory by firmware.
    + `PAGE_GRAN`  base page granule, used for section alignment
    + `ENTRY_POINT` the entry point of kernel, where the firmware-kernel hand-off happened.
    + `KEXEC_BASE` base virtual address of kernel, this define the boundary of kernel and user space.
+ `boot_secs.ldx` Define the sections used by boot code, these section should be normally used without virtual address relocation. **Important: Implementation must not define variable-sized section such as .bss**

### Booting and Initialising

Lunaix will always start the execution from entry point defined by `ENTRY_POINT` (see above section). A boot protocol might be invoked as earlier as possible in this stage. Such protocol defines the format and method to perform message exchange when firmware hand-over the control to the kernel. It is the implementation's responsibility for to decide whether such protocol is necessary and implement it accordingly.

After the completion of architectural tasks defined in boot stage, implementation must hand-over the control to the architecture-agnostic kernel entry-point: `kernel_bootstrap`, together with the an architecture-agnostically defined boot message structure: `struct boot_handoff` as the only parameter taken by the entry-point. In this structure, the implementation must provide **either** the following information:

1. `mem` sub-structure and pointer and length to the kernel command-line raw string.
2. `dtb_pa`, physical pointer to the devicetree blob (dtb) placed in memory. Fields stated in #1 will be ignored if `dtb_pa` is non-zero.

Implementation may also provide callback to `release` and `prepare` function pointer. Where the implementation-defined action can be performed before and after the kernel initialisation.

### System-on-Chip Resources

Most of the SoC resources such as interrupt controller can be abstracted away by some mandatory headers defined in `asm/`, or as a regular device driver integrated into the Lunaix device subsystem. However, not all SoC resources can be fit into the said framework. The following list the such resources:

+ **System Timer** 

### Architectural Optimisation on klibc

Most functions in `klibc` can be override by an architectural port. To achieve
this, one just need to add definition of such function. This allow port to
provide better performance on these library functions by exploiting the
architectural specific feature.

All non-static function from these header can be overriden:

+ `klibc/string.h`
+ `klibc/crc.h`
+ `klibc/hash.h`
