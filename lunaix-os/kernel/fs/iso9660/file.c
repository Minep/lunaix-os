#include <lunaix/fs.h>
#include <lunaix/fs/iso9660.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

#include <klibc/string.h>

int
iso9660_open(struct v_inode* this, struct v_file* file)
{
    // TODO
    return 0;
}

int
iso9660_close(struct v_file* file)
{
    // TODO
    return 0;
}

int
iso9660_read(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    // This read implementation handle both interleaved and non-interleaved
    // structuring

    struct iso_inode* isoino = inode->data;
    struct device* bdev = inode->sb->dev;

    len = MIN(fpos + len, inode->fsize);
    if (len <= fpos) {
        return 0;
    }

    len -= fpos;

    size_t fu_len = isoino->fu_size * ISO9660_BLKSZ;
    // if fpos is not FU aligned, then we must do an extra read.
    size_t fu_to_read =
      ICEIL(len, fu_len) + (fpos > fu_len && (fpos % fu_len) != 0);
    size_t sec = (fpos % fu_len) / ISO9660_BLKSZ;
    size_t wd_start = fpos % ISO9660_BLKSZ,
           wd_len = MIN(len, ISO9660_BLKSZ - wd_start), i = 0;

    // how many blocks (file unit + gaps) before of our current read position
    size_t true_offset = (fpos / fu_len);
    true_offset = true_offset * (isoino->fu_size + isoino->gap_size);

    void* rd_buffer = valloc(ISO9660_BLKSZ);

    true_offset += sec + inode->lb_addr;

    int errno = 0;
    while (fu_to_read) {
        for (; sec < isoino->fu_size && i < len; sec++) {
            errno = bdev->read(
              bdev, rd_buffer, true_offset * ISO9660_BLKSZ, ISO9660_BLKSZ);

            if (errno < 0) {
                errno = EIO;
                goto done;
            }

            memcpy(buffer + i, rd_buffer + wd_start, wd_len);

            i += wd_len;
            true_offset++;
            wd_start = 0;
            wd_len = MIN(len - i, ISO9660_BLKSZ);
        }
        sec = 0;
        fu_to_read--;
    }
    errno = i;

done:
    vfree(rd_buffer);
    return errno;
}

int
iso9660_write(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    // TODO
    return ENOTSUP;
}

int
iso9660_seek(struct v_inode* inode, size_t offset)
{
    // TODO
    return 0;
}