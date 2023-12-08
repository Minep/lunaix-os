<p align="center">
  <img width="auto" src="docs/img/lunaix-os-logo.png">
</p>

<p align="center">
  <a href="#lunaixos-project">简体中文</a> | <a href="docs/README_en.md">English</a>
</p>

# The LunaixOS Project

LunaixOS - 一个简单的，详细的，POSIX兼容的（但愿！），带有浓重个人风格的操作系统。开发过程以视频教程形式在Bilibili呈现：[《从零开始自制操作系统系列》](https://space.bilibili.com/12995787/channel/collectiondetail?sid=196337)。

## 1. 一些实用资源

如果有意研读LunaixOS的内核代码和其中的设计，以下资料可能会对此有用。

+ [内核虚拟内存的详细布局](docs/img/lunaix-os-mem.png)
+ [LunaixOS启动流程概览](docs/img/boot_sequence.jpeg)
+ LunaixOS总体架构概览（WIP）

## 2. 当前进度以及支持的功能

该操作系统支持x86架构，运行在保护模式中，采用宏内核架构，目前仅支持单核心。架构与内核的解耦合工作正在进行中。

在下述列表中，则列出目前所支持的所用功能和特性。列表项按照项目时间戳进行升序排列。

+ 使用Multiboot进行引导启动
+ APIC/IOAPIC作为中断管理器和计时器
+ ACPI
+ 虚拟内存
+ 内存管理与按需分页
+ 键盘输入
+ 多进程
+ 54个常见的Linux/POSIX系统调用（[附录1](#appendix1)）
+ 用户模式
+ 信号机制
+ PCI 3.0
+ PCIe 1.1 (WIP)
+ Serial ATA AHCI
+ 文件系统
  + 虚拟文件系统
  + ISO9660
    + 原生
    + Rock Ridge拓展
+ 远程GDB串口调试 (COM1@9600Bd)
+ 用户程序加载与执行
+ 动态链接 (WIP)
+ 通用设备抽象层
+ 通用图形设备抽象层
  + 标准VGA实现
+ 虚拟终端设备接口（兼容 POSIX.1-2008）

已经测试过的环境：

+ QEMU (>=7.0.0)
+ Bochs（SATA功能不支持）
+ Virtualbox
+ Dell G3 3779

## 3. 目录结构

|                                           |                              |
| ----------------------------------------- | ---------------------------- |
| [lunaix-os](lunaix-os/)                   | LunaixOS源代码               |
| [slides](slides/)                         | 视频中所用的幻灯片和补充材料 |
| [reference-material](reference-material/) | 标准，技术文档和参考文献     |

## 4. 编译与构建

构建该项目需要满足以下条件：

+ gcc 工具链
+ make
+ xorriso
+ grub-mkrescue

### 4.1 使用 GNU CC 工具链

正如同大多数OS一样，LunaixOS 是一个混合了 C 和汇编的产物。这就意味着你得要使用一些标准的C编译器来构建Lunaix。在这里，我推荐使用 GNU CC 工具链来进行构建。至于其他的工具链，如llvm，也可以去尝试，但对此我就不能作任何的保证了。

如果你使用的是基于 x86 指令集的Linux系统，不论是64位还是32位，**其本机自带的gcc就足以编译Lunaix**。 当然了，如果说你的平台是其他非x86的，你也可以指定使用某个针对x86_32的gcc套件来进行交叉编译——在`make`时通过`CX_PREFIX`变量来指定gcc套件的前缀。如下例所示，我们可以在任意平台上，如risc-v，单独使用一个面向x86_32的gcc来进行交叉编译：

```
make CX_PREFIX=i686-linux-gnu- all
```

由于目前Lunaix仅支持x86_32微架构， `CX_PREFIX` 指向的gcc必须具有针对x86_32架构进行交叉编译的能力。

### 4.2 Docker镜像

对于开发环境，本项目也提供了Docker镜像封装。开箱即用，无需配置，非常适合懒人或惜时者。详细使用方法请转到：[Lunaix OSDK项目](https://github.com/Minep/os-devkit)。

### 4.3 构建选项

假若条件满足，那么可以直接执行`make all`进行构建，完成后可在生成的`build`目录下找到可引导的iso。

本项目支持的make命令：
| 命令                     | 用途                                            |
| ------------------------ | ----------------------------------------------- |
| `make all`               | 构建镜像（`-O2`，但禁用CSE相关的优化项 **※** ） |
| `make instable`          | 构建镜像（`-O2`，开启CSE相关优化）              |
| `make all-debug`         | 构建适合调试用的镜像（`-Og`）                   |
| `make run`               | 使用QEMU运行build目录下的镜像                   |
| `make debug-qemu`        | 构建并使用QEMU进行调试                          |
| `make debug-bochs`       | 构建并使用Bochs进行调试                         |
| `make debug-qemu-vscode` | 用于vscode整合                                  |
| `make clean`             | 删除build目录                                   |

**※：由于在`-O2`模式下，GCC会进行CSE优化，这导致LunaixOS会出现一些非常奇怪、离谱的bug，从而影响到基本运行。具体原因有待调查。**

## 5. 运行，分支以及 Issue

### 5.1 虚拟磁盘（非必须）

你可以绑定一个虚拟磁盘镜像，可以使用如下命令快速创建一个：

```bash
qemu-img create -f vdi machine/disk0.vdi 128M
```

如果你想要使用别的磁盘镜像，需要修改`configs/make-debug-tool`

找到这一行：

```
-drive id=disk,file="machine/disk0.vdi",if=none \
```

然后把`machine/disk0.vdi`替换成你的磁盘路径。

有很多办法去创建一个虚拟磁盘，比如[qemu-img](https://qemu-project.gitlab.io/qemu/system/images.html)。

### 5.2 代码稳定性

主分支一般是稳定的。因为在大多数情况下，我都会尽量保证本机运行无误后，push到该分支中。至于其他的分支，则是作为标记或者是开发中的功能。前者标记用分支一般会很快删掉；后者开发分支不能保证稳定性，这些分支的代码有可能没有经过测试，但可以作为Lunaix当前开发进度的参考。

该系统是经过虚拟机和真机测试。如果发现在使用`make all`之后，虚拟机中运行报错，则一般是编译器优化问题。这个问题笔者一般很快就会修复，如果你使用别的版本的gcc（笔者版本11.2），出现了此问题，欢迎提issue。请参考[附录3：Issue的提交](#appendix3)

下面列出一些可能会出现的问题。

#### 问题#1： QEMU下8042控制器提示找不到

这是QEMU配置ACPI时的一个bug，在7.0.0版中修复了。

#### 问题#2：多进程运行时，偶尔会出现General Protection错误

这很大概率是出现了竞态条件。虽然是相当不可能的。但如果出现了，还是请提issue。

#### 问题#3：Bochs无法运行，提示找不到AHCI控制器

正常，**因为Bochs不支持SATA**。请使用QEMU或VirtualBox。

## 6. 调试 Lunaix 内核

除了[附录4：串口GDB远程调试](#appendix4)描述的一种用于实机调试的方式以外。LunaixOS还提供了LunaDBG调试套件。这是一个GDB客户端插件，包含了对GDB原生命令集的一些扩充，主要用于改善与简化内核调试的过程。目前包含以下几个命令：

+ `vmrs [pid]` 列举进程`<pid>`的内存区域图（Memory Regions），如果`<pid>`未指定，则默认为正在运行的进程（smp=1）。
+ `proc [pid]` 打印进程`<pid>`的进程控制块状态，如果`<pid>`未指定，则默认为正在运行的进程（smp=1）。
+ `proc_table` 列举所有非终止的进程以及他们的状态。

该插件可以通过运行以下命令来进行安装：

```shell
./scripts/gdb/install_lunadbg
```

## 7. 参考教程

**没有！！** 本教程以及该操作系统均为原创，没有基于任何市面上现行的操作系统开发教程，且并非是基于任何的开源内核的二次开发。

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

+ [OSDev](https://wiki.osdev.org/Main_Page) - 杂七杂八的参考，很多过来人的经验。作者主要用于上古资料查询以及收集；技术文献，手册，标准的粗略总结；以及开发环境/工具链的搭建。
+ [FreeVGA](http://www.osdever.net/FreeVGA/home.htm) - 98年的资源！关于VGA编程技术的宝藏网站。
+ GNU CC 和 GNU LD 的官方文档。
+ [PCI Lookup](https://www.pcilookup.com/) - PCI设备编号查询

#### 其他

+ Linux Manual - 用于查询*nix API的一些具体行为。

## 附录1：支持的系统调用<a id="appendix1"></a>

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
2. `rmdir(2)`※
2. `unlink(2)`※
2. `unlinkat(2)`※
2. `link(2)`※
2. `fsync(2)`※
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

**LunaixOS自有**

1. `yield`
2. `geterrno`
3. `realpathat`
4. `syslog`
5. `pollctl`

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
