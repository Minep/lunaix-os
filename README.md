<p align="center">
  <img width="auto" src="docs/img/lunaix-os-logo.png">
</p>

<p align="center">
  <a href="#lunaixos-project">简体中文</a> | <a href="docs/README_en.md">English</a>
</p>

# The LunaixOS Project

LunaixOS - 一个简单的，详细的，POSIX兼容的（但愿！），带有浓重个人风格的操作系统，由 Lunaix 内核驱动。开发过程以视频教程形式在Bilibili呈现：[《从零开始自制操作系统系列》](https://space.bilibili.com/12995787/channel/collectiondetail?sid=196337)。

## 1. 一些实用资源

如果有意研读 Lunaix 内核代码和其中的设计，或欲开始属于自己的OS开发之道，以下资料可能会对此有用。

+ [LunaixOS源代码分析教程](docs/tutorial/0-教程介绍和环境搭建.md)
+ 内核虚拟内存的详细布局
  + [x86_32](docs/img/lunaix-mem-map/lunaix-mem-x86_32.png)
  + [x86_64](docs/img/lunaix-mem-map/lunaix-mem-x86_64.png)
+ [LunaixOS启动流程概览](docs/img/boot_sequence.jpeg)
+ LunaixOS总体架构概览（WIP）
+ [作者修改的QEMU](https://github.com/Minep/qemu) (添加了一些额外用于调试的功能)

## 2. 当前进度以及支持的功能

Lunaix内核具有支持多种不同的指令集架构的能力，目前支持如下：

+ x86_32
+ x86_64

Lunaix全部特性一览：

+ 使用Multiboot进行引导启动
  + Multiboot 1
  + Multiboot 2 (WIP)
+ APIC/IOAPIC作为中断管理器和计时器
+ ACPI
+ 虚拟内存
  + 架构中性设计
  + 按需分页
  + Copy-on-Write
+ 内存管理
+ 进程模型
+ 61个常见的Linux/POSIX系统调用（[附录1](#appendix1)）
+ 用户/内核态隔离
+ 信号机制
+ PCI 3.0
+ PCIe 1.1 (WIP)
+ 块设备IO与驱动
  + 块IO通用缓存池
  + Serial ATA AHCI
    + ATA设备
    + ATAPI封装的SCSI协议
+ 文件系统（POSIX.1-2008, section 5 & 10）
  + 虚拟文件系统
  + 内核态文件系统（twifs, Lunaix自己的sysfs）
  + 设备文件系统（devfs, Lunaix自己的udev）
  + 进程文件系统（procfs）
  + ISO9660
    + ECMA-119
    + IEEE P1282（Rock Ridge拓展）
  + ext2
    + Revision 0
    + Revision 1 （额外特性不支持）
+ 远程GDB串口调试 (COM1@9600Bd)
+ 用户程序加载与执行
+ 通用设备抽象层
  + 架构中性的设备支持位于：`lunaix-os/hal`
    + 16550 UART
    + ACPI （不完全实现）
  + 架构耦合的设备支持位于：`lunaix-os/arch/<ARCH>/hal`
    + x86
      + APIC/IOAPIC 组合
      + MC146818 RTC
      + i8042 PS/2
      + RNG（使用`rdrand`）
+ 通用图形设备抽象层 (Draft)
  + 参考：`lunaix-os/hal/gfxa`
+ 虚拟终端设备接口（POSIX.1-2008, section 11）
  + 参考：`lunaix-os/hal/term`
+ 线程模型
  + 用户线程支持（pthread系列）
  + 内核线程支持
+ 抢占式内核设计
  + 内核态上下文切换
  + 内核态异常挂起/死锁自动检测机制

## 3. 目录结构

|                                           |                              |
| ----------------------------------------- | ---------------------------- |
| [lunaix-os](lunaix-os/)                   | LunaixOS源代码               |
| [slides](slides/)                         | 视频中所用的幻灯片和补充材料 |
| [reference-material](reference-material/) | 标准，技术文档和参考文献     |

## 4. 编译与构建

**！如果想要立刻构建并运行，请参考4.6！**

构建该项目需要满足以下条件：

+ gcc 工具链
+ make
+ xorriso
+ grub-mkrescue

### 4.1 使用 GNU CC 工具链

正如同大多数内核一样，Lunaix 是一个混合了 C 和汇编的产物。这就意味着你得要使用一些标准的C编译器来构建Lunaix。在这里，我推荐使用 GNU CC 工具链来进行构建。因为Lunaix 在编写时使用了大量的GNU CC 相关编译器属性修饰 (`__attribute__`) 。假若使用其他工具链，如LLVM，我对此就不能做出任何保证了。

如果你使用的是基于 x86 指令集的Linux系统，不论是64位还是32位，**其本机自带的gcc就足以编译Lunaix**。 当然了，如果说你的平台是其他非x86的，你也可以指定使用某个针对x86_32的gcc套件来进行交叉编译——在`make`时通过`CX_PREFIX`变量来指定gcc套件的前缀。如下例所示，我们可以在任意平台上，如risc-v，单独使用一个面向x86_32的gcc来进行交叉编译：

```
make CX_PREFIX=i686-linux-gnu- all
```

### 4.2 Docker镜像

对于开发环境，本项目也提供了Docker镜像封装。开箱即用，无需配置，非常适合懒人或惜时者。详细使用方法请转到：[Lunaix OSDK项目](https://github.com/Minep/os-devkit)。

### 4.3 构建选项


假若条件满足，那么可以直接执行`make all`进行构建，完成后可在生成的`build`目录下找到可引导的iso。

本项目支持的make命令：
| 命令                     | 用途                                            |
| ------------------------ | ----------------------------------------------- |
| `make all`               | 等价于 `make image` |
| `make image`             | 构建ISO镜像，可直接启动，使用ISO9660文件系统         |
| `make kernel`            | 构建内核ELF镜像，无法直接启动，需要引导程序        |
| `make clean`             | 删除构建缓存，用于重新构建               |
| `make config`            | 配置Lunaix                                   |

与make命令配套的环境变量，Lunaix的makefile会自动检测这些环境变量，以更改构建行为

+ `MODE={debug|release}` 使用debug模式构建（-Og）或者release模式（-O2）
+ `ARCH=<isa>` 为指定的指令集架构编译Lunaix。 所使用的配置选项均为选定架构默认，该环境变量
  存在的目的就是方便用户进行快速编译，而无需钻研Lunaix的种种配置项。

### 4.4 Lunaix的功能配置

Lunaix是一个可配置的内核，允许用户在编译前选择应当包含或移除的功能。

使用`make config`来进行基于命令行的交互配置。呈现方式采用Shell的形式，所有的配置项按照类似于文件树的形式组织，如单个配置项为一个“文件”，多个配置项组成的配置组为一个目录，呈现形式为方括号`[]`包裹起来的项目。在提示符中输入`usage`并回车可以查看具体的使用方法。

一个最常用的配置可能就是`architecture_support/arch`了，也就是配置Lunaix所面向的指令集。比如，编译一个在x86_64平台上运行的Lunaix，在提示符中输入（**注意等号两侧的空格，这是不能省略的**）：

```
/architecture_support/arch = x86_64
```

之后输入`exit`保存并推出。而后正常编译。


### 4.5 设置内核启动参数

在 make 的时候通过`CMDLINE`变量可以设置内核启动参数列表。该列表可以包含多个参数，通过一个或多个空格来分割。每个参数可以为键值对 `<key>=<val>` 或者是开关标志位 `<flag>`。目前 Lunaix 支持以下参数：

+ `console=<dev>` 设置系统终端的输入输出设备（tty设备）。其中 `<dev>` 是设备文件路径 （注意，这里的设备文件路径是针对Lunaix，而非Linux）。关于LunaixOS设备文件系统的介绍可参考 Lunaix Wiki（WIP）

如果`CMDLINE`未指定，则将会载入默认参数：

```
console=/dev/ttyFB0
```

其中，`/dev/ttyFB0` 指向基于VGA文本模式的tty设备，也就是平时启动QEMU时看到的黑色窗口。

当然，读者也可以使用 `/dev/ttyS0` 来作为默认tty设备，来验证 Lunaix 的灵活性与兼容性。该路径指向第一个串口设备。可以通过telnet协议在`12345`端口上进行访问——端口号可以自行修改QEMU启动参数（位于：`makeinc/qemu.mkinc`）来变更。

**注意：** 根据操作系统和键盘布局的不同，telnet客户端对一些关键键位的映射（如退格，回车）可能有所差别（如某些版本的Linux会将退格键映射为`0x7f`，也就是ASCII的`<DEL>`字符，而非我们熟知`0x08`）。如果读者想要通过串口方式把玩Lunaix，请修改`usr/init/init.c`里面的终端初始化代码，将`VERASE`设置为正确的映射（修改方式可以参考 POSIX termios 的使用方式。由于Lunaix的终端接口的实现是完全兼容POSIX的，读者可以直接去查阅Linux自带的帮助`man termios`，无需作任何的转换）

### 4.6 测试与体验 Lunaix

用户可以使用脚本`live_debug.sh` 来快速运行Lunaix。 该脚本自动按照默认的选项构建Lunaix，而后调用 `scripts/qemu.py` 根据配置文件生成QEMU启动参数
（配置文件位于`scripts/qemus/`）

由于该脚本的主要用途是方便作者进行调试，所以在QEMU窗口打开后还需要进行以下动作：

1. 使用telnet连接到`localhost:12345`，这里是Lunaix进行标准输入输出所使用的UART映射（QEMU为guest提供UART实现，并将其利用telnet协议重定向到宿主机）
2. 在GDB窗口中输入`c`然后回车，此时Lunaix开始运行。这样做的目的是允许在QEMU进行模拟前，事先打好感兴趣的断点。

该脚本的运行需要设置 `ARCH=<isa>` 环境变量，其值需要与编译时制定的值一致。

如：

```sh
ARCH=x86_64 ./live_debug.sh
```

## 5. 运行，分支以及 Issue

### 5.1 代码稳定性

主分支一般是稳定的。因为在大多数情况下，我都会尽量保证本机运行无误后，push到该分支中。至于其他的分支，则是作为标记或者是开发中的功能。前者标记用分支一般会很快删掉；后者开发分支不能保证稳定性，这些分支的代码有可能没有经过测试，但可以作为Lunaix当前开发进度的参考。

该系统是经过虚拟机和真机测试。如果发现在使用`make all`之后，虚拟机中运行报错，则一般是编译器优化问题。这个问题笔者一般很快就会修复，如果你使用别的版本的gcc（笔者版本11.2），出现了此问题，欢迎提issue。请参考[附录3：Issue的提交](#appendix3)

## 6. 调试 Lunaix 内核

除了[附录4：串口GDB远程调试](#appendix4)描述的一种用于实机调试的方式以外。LunaixOS还提供了LunaDBG调试套件。这是一个GDB客户端插件，包含了对GDB原生命令集的一些扩充，主要用于改善与简化内核调试的过程。目前包含以下几个命令：

+ `vmrs [pid]` 列举进程`<pid>`的内存区域图（Memory Regions），如果`<pid>`未指定，则默认为正在运行的进程（smp=1）。
+ `proc [pid]` 打印进程`<pid>`的进程控制块状态，如果`<pid>`未指定，则默认为正在运行的进程（smp=1）。
+ `sched <threads | procs> [-l]` 查看调度器信息，接受两个参数：
  + `threads` 打印所有依然在调度器中有注册的线程
  + `procs` 打印所有依然在调度器中有注册的进程
  + 可选开关 `-l` 决定是否以长列表打印（更详细的信息）

该插件可以通过运行以下命令来进行安装：

```shell
./scripts/gdb/install_lunadbg
```

## 7. 参考教程

#### 没有！！

本教程以及该操作系统的所有的架构设计与实现**均为原创**。

对此，作者可以保证，该项目是做到了三个 “没有”：

+ **没有** 参考任何现行的，关于操作系统开发的，教程或书籍。
+ **没有** 参考任何开源内核的源代码（包括Linux）
+ **没有** 基于任何开源内核的二次开发行为。

为了制作LunaixOS，作者耗费大量时间和精力钻研技术文档，手册，理论书籍以及现行工业标准，从而尽量保证了知识的一手性。（这样一来，读者和听众们也算是拿到了二手的知识，而不是三手，四手，甚至n手的知识）。

大部分的文档和标准可以在上述的[reference-material](reference-material/)中找到。

当然，您也可以参考以下列表来了解现阶段的LunaixOS都使用了哪些资料（本列表会随着开发进度更新）：

#### 手册，标准，技术文档

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
+ Tool Interface Standard (TIS) Portable Formats Specification (Version 1.1)

**免责声明：PCI相关的标准最终解释权归PCI-SIG所有。此处提供的副本仅供个人学习使用。任何商用目的须向PCI-SIG购买。**

#### 理论书籍

+ *Computer System - A Programmer's Perspective Third Edition (CS:APP)* (Bryant, R & O'Hallaron, D)
+ *Modern Operating System* (Tanenbaum, A)

#### 网站

+ [OSDev](https://wiki.osdev.org/Main_Page) - 杂七杂八的参考，很多过来人的经验。作者主要用于上古资料查询以及收集；技术文献，手册，标准的粗略总结；以及开发环境/工具链的搭建。当然，上面的内容假设了x86_32架构的生态，对于其他的ISA支持，该网站便失去了其价值了。
+ [FreeVGA](http://www.osdever.net/FreeVGA/home.htm) - 98年的资源！关于VGA编程技术的宝藏网站。
+ GNU CC 和 GNU LD 的官方文档。
+ [PCI Lookup](https://www.pcilookup.com/) - PCI设备编号查询

#### 其他

+ Linux Manual - 用于查询*nix API的一些具体行为。

## 附录1：实现的 POSIX 系统接口 <a id="appendix1"></a>

LunaixOS 提供对以下POSIX的系统接口的实现。内核定义的系统调用号可以参考 [LunaixOS系统调用表](docs/lunaix-syscall-table.md) 。

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
1. `sigaction(2)`
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
2. `readlink(2)`
2. `readlinkat(2)`
2. `rmdir(2)`
2. `unlink(2)`
2. `unlinkat(2)`
2. `link(2)`※
2. `fsync(2)`
2. `dup(2)`
2. `dup2(2)`
2. `symlink(2)`
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


( **※**：该系统调用暂未经过测试 )

## 附录2：Issue的提交<a id="appendix3"></a>

假若Lunaix的运行出现任何问题，还请按照以下的描述，在Issue里面提供详细的信息。

+ 可用于复现问题的描述和指引（如Lunaix运行平台的软硬件配置）
+ 错误症状描述
+ LunaixOS在panic时打印的调试信息（如无法复制，可以截图）

## 附录3：串口GDB远程调试<a id="appendix4"></a>

**（该功能正在重构，目前不可用）**

LunaixOS内核集成了最基本的GDB远程调试服务器。可通过串口COM1在9600波特率上与之建立链接。但是，在将GDB与内核链接起来之前，还需要让内核处在调试模式下。

要进入调试模式，需要往串口（波特率如上）写入字节串 `0x40` `0x63` `0x6D` `0x63`。此时，如果屏幕底部出现一条品红色背景的`DEBUG` 字样，那么就说明LunaixOS已处在调试模式下。

注意，在这个时候，LunaixOS会开始在`COM1`上监听GDB协议信息，并且暂停一切的活动（如调度，以及对外部中断的一切响应）。用户此时需要将GDB与其挂载，并使用GDB的工作流来指示内核下一步的动作。

在目前，为了防止代码过于臃肿，LunaixOS实现的是GDB远程协议要求的最小服务端命令子集：`g`, `G`, `p`, `P`, `Q`, `S`, `k`, `?`, `m`, `M`, `X`。足以满足大部分的调试需求。

当结束调试的时候，请使用GDB的`kill`指令进行连接的断开。注意，这个指令会使得LunaixOS恢复所有暂停的活动，进入正常的运行序列，但并不会退出调试模式。GDB的挂载请求依然在LunaixOS中享有最高优先权。如果需要退出调试模式，需要往串口写入字节串：`0x40` `0x79` `0x61` `0x79`。
