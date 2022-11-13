/**
 * @file iso9660.h
 * @author Lunaixsky (zelong56@gmail.com)
 * @brief ISO9660 File system header file. (Reference: ECMA-119, 4th ed.)
 * @version 0.1
 * @date 2022-10-03
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef __LUNAIX_ISO9660_H
#define __LUNAIX_ISO9660_H

#include <lunaix/clock.h>
#include <lunaix/device.h>
#include <lunaix/fs.h>
#include <lunaix/types.h>

#define ISO_SIGNATURE_LO 0x30304443UL
#define ISO_SIGNATURE_HI 0x31

// Volume Types
#define ISO_VOLBOOT 0   // Boot Record
#define ISO_VOLPRIM 1   // Primary
#define ISO_VOLSUPP 2   // Supplementary
#define ISO_VOLPART 3   // Partition
#define ISO_VOLTERM 255 // Volume descriptor set terminator

#define ISO_FHIDDEN 0x1   // a hidden file
#define ISO_FDIR 0x2      // a directory file
#define ISO_FASSOC 0x4    // a associated file
#define ISO_FRECORD 0x8   // file store in iso record fashion
#define ISO_FPROTECT 0x10 // file being protected by access control
#define ISO_FEXTENTS 0x80 // the extent by this record is a file partial

#define ISO9660_BLKSZ 2048
#define ISO9660_IDLEN 256

// NOTES:
// Each Descriptor sized 1 logical block (2048 bytes in common cases)
// ISO9660 store number in both-byte order. That is, for a d-bits number, it
//   will result in 2d bits of storage. The lower d-bits are little-endian and
//   upper d-bits are big-endian.

struct iso_vol
{
    u8_t type;
    u8_t std_id[5]; // CD001
    u8_t version;
} PACKED;

struct iso_vol_boot
{
    struct iso_vol header;
    u8_t sys_id[32];
    u8_t boot_id[32];
    u8_t reserved; // align to data line width
} PACKED;

struct iso_datetime
{
    u8_t year[4];
    u8_t month[2];
    u8_t day[2];
    u8_t hour[2];
    u8_t min[2];
    u8_t sec[2];
    u8_t ms[2];
    u8_t gmt;
} PACKED;

// 32bits both-byte-order integer
typedef struct iso_bbo32
{
    u32_t le; // little-endian
    u32_t be; // big-endian
} PACKED iso_bbo32_t;

// 16bits both-byte-order integer
typedef struct iso_bbo16
{
    u16_t le; // little-endian
    u16_t be; // big-endian
} PACKED iso_bbo16_t;

// (8.4) Describe a primary volume space
struct iso_vol_primary
{
    struct iso_vol header;
    u8_t reserved_1;
    u8_t sys_id[32];
    u8_t vol_id[32];
    u8_t reserved_2[8];
    iso_bbo32_t vol_size;
    u8_t reserved_3[32];
    iso_bbo16_t set_size;
    iso_bbo16_t seq_num;
    iso_bbo16_t lb_size;
    iso_bbo32_t ptable_size;
    u32_t lpath_tbl_ptr; // Type L Path table location (LBA)
    u32_t reserved_4[3]; // use type M if big endian machine.
    u8_t root_record[34];
    u8_t set_id[128];
    u8_t publisher_id[128];
    u8_t preparer_id[128];
    u8_t app_id[128];
    u8_t copyright_id[128];
    u8_t asbtract_id[128];
    u8_t bib_id[128];
    struct iso_datetime ctime;   // creation
    struct iso_datetime mtime;   // modification
    struct iso_datetime ex_time; // expiration
    struct iso_datetime ef_time; // effective
    u8_t fstruct_ver;            // file structure version, don't care!
} PACKED;                        // size 1124

// Layout for Supplementary Vol. is almost identical to primary vol.
// We ignore it for now. (see section 8.5, table 6)

// (8.6) Describe a volume partition within a volume space
struct iso_partition
{
    struct iso_vol header;
    u8_t reserved;
    u8_t sys_id[32];
    u8_t part_id[32];
    iso_bbo32_t part_addr;
    iso_bbo32_t part_size;
} PACKED;

// (6.10.4) MDU with variable record
struct iso_var_mdu
{
    u8_t len;
    u8_t content[0];
} PACKED;

struct iso_datetime2
{
    u8_t year;
    u8_t month;
    u8_t day;
    u8_t hour;
    u8_t min;
    u8_t sec;
    u8_t gmt;
} PACKED;

// (9.1) Directory Record [Embedded into Variable MDU]
struct iso_drecord
{
    u8_t xattr_len;
    iso_bbo32_t extent_addr;
    iso_bbo32_t data_size;
    struct iso_datetime2 PACKED mktime; // Time the record is made, see 9.1.5
    u8_t flags;
    u8_t fu_sz;  // size of file unit (FU)
    u8_t gap_sz; // size of gap if FU is interleaved.
    iso_bbo16_t vol_seq;
    struct iso_var_mdu name;
} PACKED;

struct iso_xattr
{
    iso_bbo16_t owner;
    iso_bbo16_t group;
    u16_t perm;
    struct iso_datetime ctime;
    struct iso_datetime mtime;
    struct iso_datetime ex_time;
    struct iso_datetime ef_time;
    u8_t record_fmt;
    u8_t record_attr;
    iso_bbo16_t record_len;
    u32_t sys_id;
    u8_t reserved1[64];
    u8_t version;
    u8_t len_esc;
    u8_t reserved2[64];
    iso_bbo16_t payload_sz;
    u8_t payload[0];
    // There is also a escape sequence after payload,
    // It however marked as optional, hence we ignore it.
} PACKED;

///
/// -------- IEEE P1281 SUSP ---------
///

#define ISOSU_ER 0x5245
#define ISOSU_ST 0x5453

struct isosu_base
{
    u16_t signature;
    u8_t length;
    u8_t version;
} PACKED;

struct isosu_er
{
    struct isosu_base header;
    u8_t id_len;
    u8_t des_len;
    u8_t src_len;
    u8_t ext_ver;
    u8_t id_des_src[0];
} PACKED;

///
/// -------- Rock Ridge Extension --------
///

#define ISORR_PX 0x5850
#define ISORR_PN 0x4e50
#define ISORR_SL 0x4c53
#define ISORR_NM 0x4d4e
#define ISORR_TF 0x4654

#define ISORR_NM_CONT 0x1

#define ISORR_TF_CTIME 0x1
#define ISORR_TF_MTIME 0x2
#define ISORR_TF_ATIME 0x4
#define ISORR_TF_LONG_FORM 0x80

struct isorr_px
{
    struct isosu_base header;
    iso_bbo32_t mode;
    iso_bbo32_t link;
    iso_bbo32_t uid;
    iso_bbo32_t gid;
    iso_bbo32_t sn;
} PACKED;

struct isorr_pn
{
    struct isosu_base header;
    iso_bbo32_t dev_hi;
    iso_bbo32_t dev_lo;
} PACKED;

struct isorr_sl
{
    struct isosu_base header;
    u8_t flags;
    char symlink[0];
} PACKED;

struct isorr_nm
{
    struct isosu_base header;
    u8_t flags;
    char name[0];
} PACKED;

struct isorr_tf
{
    struct isosu_base header;
    u8_t flags;
    char times[0];
} PACKED;

///
/// -------- VFS integration ---------
///

struct iso_inode
{
    u32_t record_fmt;
    u32_t fu_size;
    u32_t gap_size;
    struct llist_header drecaches;
};

struct iso_drecache
{
    struct llist_header caches;
    u32_t extent_addr;
    u32_t data_size;
    u32_t xattr_len;
    u32_t fno;
    u32_t fu_size;
    u32_t gap_size;
    u32_t flags;
    time_t ctime;
    time_t atime;
    time_t mtime;
    struct hstr name;
    char name_val[ISO9660_IDLEN];
};

struct iso_superblock
{
    u32_t volume_size;
    u32_t lb_size;
};

struct iso_drecord*
iso9660_get_drecord(struct iso_var_mdu* drecord_mdu);

int
iso9660_fill_inode(struct v_inode* inode, struct iso_drecache* dir, int ino);

time_t
iso9660_dt2unix(struct iso_datetime* isodt);

time_t
iso9660_dt22unix(struct iso_datetime2* isodt2);

void
iso9660_init();

int
iso9660_setup_dnode(struct v_dnode* dnode, struct v_inode* inode);

void
iso9660_fill_drecache(struct iso_drecache* cache,
                      struct iso_drecord* drec,
                      u32_t len);

int
iso9660_dir_lookup(struct v_inode* this, struct v_dnode* dnode);

int
iso9660_readdir(struct v_file* file, struct dir_context* dctx);

int
iso9660_open(struct v_inode* this, struct v_file* file);

int
iso9660_close(struct v_file* file);

int
iso9660_read(struct v_inode* inode, void* buffer, size_t len, size_t fpos);

int
iso9660_write(struct v_inode* inode, void* buffer, size_t len, size_t fpos);

int
iso9660_seek(struct v_inode* inode, size_t offset);

int
isorr_parse_px(struct iso_drecache* cache, void* px_start);

int
isorr_parse_nm(struct iso_drecache* cache, void* nm_start);

int
isorr_parse_tf(struct iso_drecache* cache, void* tf_start);

#endif /* __LUNAIX_ISO9660_H */
