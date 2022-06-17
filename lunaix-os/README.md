# LunaixOS 源代码

我知道这个目录结构看起来相当的劝退。特别是并没有像初代Linux那种一个文件夹里就只是一堆文件的简单朴素。但至少，就我而言，把结构细分一点儿总是好的。

## 目录结构

+ `arch` 平台相关代码，LunaixOS的内核引导就在这里。
+ `hal`  硬件抽象层，存放主板相关的代码，提供了一些访问主板功能（比如CPU，计时器）的抽象
+ `includes`  所有头文件
+ `kernel` 这里就是内核了
  + `asm` 共内核使用的，且平台相关的代码。
  + `ds` 提供一些基本的数据结构支持。
  + `mm` 各类内存管理器。
  + `peripheral` 外部设备驱动（如键盘）。
  + `time` 为内核提供基本的时间，计时服务。
  + `tty` 提供基本的显存操作服务。
+ `lib` 一些内核使用的运行时库，主要提供是内核模式下的一些C标准库里的实现。
+ `link` 链接器脚本