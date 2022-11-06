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

#include <lunaix/types.h>

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

// (8.4) Describe a primary volume space
struct iso_vol_primary
{
    struct iso_vol header;
    u8_t reserved_1;
    u8_t sys_id[32];
    u8_t vol_id[32];
    u8_t reserved_2;
    u32_t sz_lo; // (8.4.8) only lower portion is valid.
    u32_t sz_hi;
    u8_t reserved_2;
    u32_t set_size;
    u32_t seq_num;
    u32_t lb_size;
    u32_t path_tbl_sz_lo; // lower partition - LE.
    u32_t path_tbl_sz_hi;
    u32_t lpath_tbl_ptr; // Type L Path table location (LBA)
    u32_t reserved_3[3]; // use type M if big endian machine.
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
    u32_t part_addr_lo; // (8.6.7) only lower portion is valid.
    u32_t part_addr_hi;
    u32_t part_sz_lo; // (8.6.8) only lower portion is valid.
    u32_t part_sz_hi;
} PACKED;

// (6.10.4) MDU with variable record
struct iso_var_mdu
{
    u8_t len;
    u8_t content[0];
} PACKED;

// (9.1) Directory Record [Embedded into Variable MDU]
struct iso_drecord
{
    u8_t xattr_len;
    u32_t extent_lo; // location of extent, lower 32 bits is valid.
    u32_t extent_hi;
    u32_t data_sz_lo; // size of extent, lower 32 bits is valid.
    u32_t data_sz_hi;
    struct
    {
        u8_t year;
        u8_t month;
        u8_t hour;
        u8_t min;
        u8_t sec;
        u8_t gmt;
    } PACKED mktime; // Time the record is made, see 9.1.5
    u8_t flags;
    u8_t fu_sz;  // size of file unit (FU)
    u8_t gap_sz; // size of gap if FU is interleaved.
    u32_t vol_seq;
    struct iso_var_mdu name;
} PACKED;

// (9.4) Path Table Record. [Embedded into Variable MDU]
struct iso_precord
{
    u8_t xattr_len;
    u32_t extent_addr;
    u8_t parent; // indexed into path table
    u8_t id[0];  // length = iso_var_mdu::len
} PACKED;

struct iso_xattr
{
    u32_t owner;
    u32_t group;
    u16_t perm;
    struct iso_datetime ctime;
    struct iso_datetime mtime;
    struct iso_datetime ex_time;
    struct iso_datetime ef_time;
    u8_t record_fmt;
    u8_t record_attr;
    u8_t record_len;
    u32_t sys_id;
    u8_t reserved1[64];
    u8_t version;
    u8_t len_esc;
    u8_t reserved2[64];
    u32_t payload_sz;
    u8_t payload[0];
    // There is also a escape sequence after payload,
    // It however marked as optional, hence we ignore it.
} PACKED;

#endif /* __LUNAIX_ISO9660_H */
