<p align="center">
  <img width="auto" src="docs/img/lunaix-os-logo.png">
</p>

<p align="center">
  <span>简体中文</span> | <a href="docs/README_en.md">English</a>
</p>

# LunaixOS Project 
LunaixOS - 一个简单的，详细的，POSIX兼容的（但愿！），带有浓重个人风格的操作系统。开发过程以视频教程形式在Bilibili呈现：[《从零开始自制操作系统系列》](https://space.bilibili.com/12995787/channel/collectiondetail?sid=196337)。

## 当前进度以及支持的功能

该操作系统支持x86架构，运行在保护模式中，采用宏内核架构，目前仅支持单核心。内存结构采用经典的3:1划分，即低3GiB为用户地址空间（0x400000 ~ 0xBFFFFFFF），内核地址空间重映射至高1GiB（0xC0000000 ~ 0xFFFFFFFF）。内存的详细布局可参考[LunaixOS内存地图](docs/img/lunaix-os-mem.png)

在下述列表中，则列出目前所支持的所用功能和特性。列表项按照项目时间戳进行升序排列。

+ 使用Multiboot进行引导启动
+ APIC/IOAPIC作为中断管理器和计时器
+ ACPI
+ 虚拟内存
+ 内存管理与按需分页（Demand Paging）
+ 键盘输入
+ 多进程
+ 17个常见的Linux/POSIX系统调用（[附录1](#appendix1)）
+ 用户模式
+ 信号机制
+ PCI 3.0
+ PCIe 2.0 (WIP)

## 目录结构

| | |
|-----|------|
| [lunaix-os](lunaix-os/) | LunaixOS源代码 |
| [slides](slides/) | 视频中所用的幻灯片和补充材料 |
| [reference-material](reference-material/)| 标准，技术文档和参考文献 |

## 参考教程

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

#### 理论书籍
+ *Computer System - A Programmer's Perspective Third Edition (CS:APP)* (Bryant, R & O'Hallaron, D)
+ *Modern Operating System* (Tanenbaum, A)
+ 《汇编语言》（王爽） - 用于入门Intel语法的x86汇编（对于AT&T语法，推荐阅读CS:APP）
+ ~~《微机原理与接口技术》 - 用于大致了解x86架构的微机体系（更加细致的了解可以阅读Intel Manual）~~ （已过时，推荐阅读CS:APP）

#### 网站
+ [OSDev](https://wiki.osdev.org/Main_Page) - 杂七杂八的参考，很多过来人的经验。作者主要用于上古资料查询以及收集；技术文献，手册，标准的粗略总结；以及开发环境/工具链的搭建。
+ [FreeVGA](http://www.osdever.net/FreeVGA/home.htm) - 98年的资源！关于VGA编程技术的宝藏网站。
+ GNU CC 和 GNU LD 的官方文档。
+ [PCI Lookup](https://www.pcilookup.com/) - PCI设备编号查询

#### 其他
+ Linux Manual - 用于查询*nix API的一些具体行为。


## 附录1：支持的系统调用<a id="appendix1"></a>

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
1. `sigpending(2)`
1. `sigsuspend(2)`

### LunaixOS自有

1. `yield`
