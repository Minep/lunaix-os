#ifndef __LUNAIX_SPIKE_H
#define __LUNAIX_SPIKE_H

// Some helper functions. As helpful as Spike the Dragon! :)

// 除法向上取整
#define CEIL(v, k)          (((v) + (1 << (k)) - 1) >> (k))

// 除法向下取整
#define FLOOR(v, k)         ((v) >> (k))

// 获取v最近的最大k倍数
#define ROUNDUP(v, k)       (((v) + (k) - 1) & ~((k) - 1))

// 获取v最近的最小k倍数
#define ROUNDDOWN(v, k)     ((v) & ~((k) - 1))

static void inline spin() {
    while(1);
}

#endif /* __LUNAIX_SPIKE_H */
