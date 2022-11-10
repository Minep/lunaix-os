#include <klibc/string.h>
#include <lunaix/blkpart_gpt.h>
#include <lunaix/block.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/status.h>
#include <lunaix/syslog.h>

#include <lib/crc.h>

#define GPT_BLKSIZE 512
#define LBA2OFF(lba) ((lba)*GPT_BLKSIZE)
#define ENT_PER_BLK (GPT_BLKSIZE / sizeof(struct gpt_entry))

#define GPTSIG_LO 0x20494645UL
#define GPTSIG_HI 0x54524150UL

static u8_t NULL_GUID[16] = { 0 };

LOG_MODULE("GPT")

int
blkpart_parse(struct device* master, struct gpt_header* header)
{
    struct block_dev* bdev = (struct block_dev*)master->underlay;
    if (!bdev)
        return ENODEV;

    int errno;
    u32_t ent_lba = (u32_t)header->ents_lba;
    struct gpt_entry* ents_parial = (struct gpt_entry*)valloc(GPT_BLKSIZE);

    for (size_t i = 0; i < header->ents_len; i++) {
        if (!(i % ENT_PER_BLK)) {
            errno = master->read(
              master, ents_parial, LBA2OFF(ent_lba++), GPT_BLKSIZE);
            if (errno < 0) {
                goto done;
            }
        }

        struct gpt_entry* ent = &ents_parial[i % ENT_PER_BLK];

        // assuming the entries are not scattered around
        if (!memcmp(ent->pguid, NULL_GUID, 16)) {
            break;
        }

        // Convert UEFI's 512B LB representation into local LBA range.
        u64_t slba_local = (ent->start_lba * GPT_BLKSIZE) / bdev->blk_size;
        u64_t elba_local = (ent->end_lba * GPT_BLKSIZE) / (u64_t)bdev->blk_size;

        kprintf("%s: guid part#%d: %d..%d\n",
                bdev->bdev_id,
                i,
                (u32_t)slba_local,
                (u32_t)elba_local);
        // we ignore the partition name, as it rarely used.
        blk_mount_part(bdev, NULL, i, slba_local, elba_local);
    }

done:
    vfree(ents_parial);
    return errno;
}

int
blkpart_probegpt(struct device* master)
{
    int errno;
    struct gpt_header* gpt_hdr = (struct gpt_header*)valloc(GPT_BLKSIZE);

    if ((errno = master->read(master, gpt_hdr, LBA2OFF(1), LBA2OFF(1))) < 0) {
        goto done;
    }

    if (*(u32_t*)&gpt_hdr->signature != GPTSIG_LO ||
        *(u32_t*)&gpt_hdr->signature[4] != GPTSIG_HI) {
        return 0;
    }

    u32_t crc = gpt_hdr->hdr_cksum;
    gpt_hdr->hdr_cksum = 0;
    if (crc32b((void*)gpt_hdr, sizeof(*gpt_hdr)) != crc) {
        kprintf(KWARN "checksum failed\n");
        // FUTURE check the backup header
        return EINVAL;
    }

    errno = blkpart_parse(master, gpt_hdr);
done:
    vfree(gpt_hdr);
    return errno;
}