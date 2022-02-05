#define MB_MAGIC 0x1BADB002
#define MB_ALIGNED_4K_MEM_MAP 0x03
#define CHECKSUM(flags)     -(MB_MAGIC+flags)