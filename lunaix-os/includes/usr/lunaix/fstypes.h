#ifndef __LUNAIX_FSTYPES_H
#define __LUNAIX_FSTYPES_H

#define F_DIR 0x0
#define F_FILE 0x1
#define F_DEV 0x2
#define F_SEQDEV 0x6
#define F_VOLDEV 0xa
#define F_SYMLINK 0x10

#define F_MFILE 0b00001
#define F_MDEV 0b01110
#define F_MSLNK 0b10000

#endif /* __LUNAIX_FSTYPES_H */
