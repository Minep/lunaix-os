# Lunaix 虚拟内存地址映射图简要

该说明将会展示 Lunaix 内核在不同架构下的虚拟内存地址资源的划分。
请参看下表：

+ [x86_32](img/lunaix-mem-map/lunaix-mem-x86_32.png)
+ [x86_64](img/lunaix-mem-map/lunaix-mem-x86_64.png)

## 图例

| 名称              | 类型 | 默认权限 | 说明 |
| ---              | --- | ---          | --- |
| `reserved`       | `PcLc` | `R`       | 保留区域（存在映射，但暂无用途，或为过时硬件/功能预留） |
| <阴影区域>        | `PcLc` | N/A       | 空洞区，没有任何用处，没有任何映射 |
| `32 bits legacy` | `PcLc` | `RW`      | 存在于 64位系统上，为32位4GiB地址资源的对等映射|
| `kstask_area`    | `PcLc` | `RW`      | 内核栈聚集地，所有内核线程的栈都来自于此 |
| `ks@threadN`     | `PcLc` | `RW`      | 第N个内核线程的栈 |
| `usr_space`      | `PcLc` | `URW`     | 用户空间 |
| `usr_exec`       | `PcLv` | `URX` | 用户程序镜像映射加载区 |
| `heap`           | `PvLv` | `URW`     | 用户堆（起始地址取决于`usr_exec`） |
| `mmaps`          | `PcLc` | `URW`     | 通用内存映射区 |
| `ustack_area`    | `PcLc` | `URW`     | 用户栈聚集地，所有用户线程的栈都来自于此 |
| `vmap`           | `PcLc`* | `RW`      | 内核通用内存映射区，一个内存地址池，为内核所有组件/子系统，提供地址资源管理服务 |
| `vms_mnt`        | `PcLc` | `RW`      | 虚拟内存空间挂载点，用于远程编辑其他虚拟内存空间的映射关系（PTE们） |
| `vms_self`       | `PcLc` | `RW`      | 环回挂载点，用于编辑当前虚拟内存空间的映射关系。 |
| `pg_mnts`        | `PcLc` | `RW`      | 页挂载点，用于临时挂载页面，以便对其内容进行读写，最多支持挂载4个标准页以及一个大小不超过512M的巨页/复合页 |
| `pmap`           | `PvLc` | `RW`      | 用于存放物理页管理结构（一个巨型数组，长度取决于实装内存大小） |
| `kernel_exec`    | `PcLc` | `RX`   | 内核镜像 |

类型注释：

+ `Pv`: 动态起始地址（Positione varia）
+ `Pc`: 固定起始地址（Positione certa）
+ `Lv`: 动态长度（Longitudinis variae）
+ `Lc`: 固定长度（Longitudinem certam）
+ `*`: vmap的起始地址在x86_32中为动态的，取决于`pmap`的长度。