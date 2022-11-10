/**
 * @file blkpart_gpt.h
 * @author Lunaixsky (lunaxisky@qq.com)
 * @brief The GUID Partition Table (GPT)
 * @version 0.1
 * @date 2022-11-09
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef __LUNAIX_BLKPART_GPT_H
#define __LUNAIX_BLKPART_GPT_H

#include <lunaix/device.h>
#include <lunaix/types.h>

struct gpt_header
{
    u8_t signature[8];
    u32_t rev;
    u32_t hdr_size;
    u32_t hdr_cksum;
    u32_t reserved1;
    u64_t hdr_lba;
    u64_t hdr_backup_lba;
    u64_t lba_start;
    u64_t lba_end;
    u8_t guid[16];
    u64_t ents_lba;
    u32_t ents_len;
    u32_t ent_size;
    u32_t ent_cksum;
    // reserved start here
} PACKED;

struct gpt_entry
{
    u8_t pguid[16];
    u8_t uguid[16];
    u64_t start_lba;
    u64_t end_lba;
    u64_t attr_flags;
    char name[72];
} PACKED;

int
blkpart_probegpt(struct device* master);

#endif /* __LUNAIX_BLKPART_GPT_H */
