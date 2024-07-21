# Porting Lunaix to Other ISAs

This document briefly describe how to add support for other ISA

## Adding Implementations

Lunaix provide bunch of headers that **MUST** be implemented.

+ Follow the structure and derive the template header files according to
  `arch/generic/includes`.
  ! You **MUST** copy the header file and add your own
  declaration & other stuffs.
  ! You **MUST NOT** remove or modify the data structure,
  function signature or any declaration that already in the template header
  files unless stated explicitly.
  ! Read the comment carefully, it may contains implementation recommendation
  which is vital to the overall correctness.

+ Add implementation to function signature defined in header files under
  `includes/lunaix/generic`

+ Add implementation of syscall dispatching (Reference: `arhc/x86/syscall.S`)  

+ Add implementation of interrupt handler dispatching  (Reference:
  `arhc/x86/exceptions/intrhnds.S`)  

+ Add implementation of context switching, signal handling. (Reference:
  `arhc/x86/exceptions/interrupt.S`)  
  **TODO: make this procedure more standalone**

## Preparing the Flows

When system boot up, Lunaix start the execution from `start_`, and then the
control flow will transfer to the `kinit.c::kernel_bootstrap`. A boot hand-over 
context `struct boot_handoff*` must passed to the bootstrap procedure.

Before jumping to `kernel_bootstrap`, one

+ Must constructure a proper `struct boot_handoff` context.
+ Must remap the kernel to architecturally favoured address range. 
    **TODO: the algorithm for doing kernel remapping should be arch-agnostic**
+ Must perform any essential CPU statet initialization, for example, setting up
  address translation scheme, enable/disable ISA extensions, probing CPU/SOC
  features.

## Alternative Implementation

Most functions in `klibc` can be override by an architectural port. To achieve
this, one just need to add definition of such function. This allow port to
provide better performance on these library functions by exploiting the
architectural specific feature.

All non-static function from these header can be overriden:

+ `klibc/string.h`
+ `klibc/crc.h`
+ `klibc/hash.h`
