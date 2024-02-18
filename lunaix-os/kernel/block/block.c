#include <klibc/strfmt.h>
#include <klibc/string.h>

#include <hal/ahci/hba.h>

#include <lib/crc.h>

#include <lunaix/blkpart_gpt.h>
#include <lunaix/block.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/pagetable.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

#define BLOCK_EREAD 1
#define BLOCK_ESIG 2
#define BLOCK_ECRC 3
#define BLOCK_EFULL 3

LOG_MODULE("BLOCK")

#define MAX_DEV 32

static struct cake_pile* lbd_pile;
static struct block_dev** dev_registry;
static struct twifs_node* blk_sysroot;
static struct device_cat* blk_parent_dev;

int free_slot = 0;

int
__block_mount_partitions(struct hba_device* hd_dev);

int
__block_register(struct block_dev* dev);

void
block_init()
{
    blkio_init();
    lbd_pile = cake_new_pile("block_dev", sizeof(struct block_dev), 1, 0);
    dev_registry = vcalloc(sizeof(struct block_dev*), MAX_DEV);
    free_slot = 0;
    blk_sysroot = twifs_dir_node(NULL, "block");
    blk_parent_dev = device_addcat(NULL, "block");
}

static int
__block_commit(struct blkio_context* blkio, struct blkio_req* req, int flags)
{
    int errno;
    blkio_commit(blkio, req, flags);

    if ((errno = req->errcode)) {
        errno = -errno;
    }

    vbuf_free(req->vbuf);
    blkio_free_req(req);
    return errno;
}

int
__block_read(struct device* dev, void* buf, size_t offset, size_t len)
{
    int errno;
    struct block_dev* bdev = (struct block_dev*)dev->underlay;
    size_t bsize = bdev->blk_size, rd_block = offset / bsize + bdev->start_lba,
           r = offset % bsize, rd_size = 0;

    if (!(len = MIN(len, ((size_t)bdev->end_lba - rd_block + 1) * bsize))) {
        return 0;
    }

    struct vecbuf* vbuf = NULL;
    struct blkio_req* req;
    void *head_buf = NULL, *tail_buf = NULL;

    // align the boundary
    if (r || len < bsize) {
        head_buf = valloc(bsize);
        rd_size = MIN(len, bsize - r);
        vbuf_alloc(&vbuf, head_buf, bsize);
    }

    // align the length
    if ((len - rd_size)) {
        size_t llen = len - rd_size;
        assert_msg(!(llen % bsize), "misalign block read");
        vbuf_alloc(&vbuf, buf + rd_size, llen);
    }

    req = blkio_vrd(vbuf, rd_block, NULL, NULL, 0);

    if (!(errno = __block_commit(bdev->blkio, req, BLKIO_WAIT))) {
        memcpy(buf, head_buf + r, rd_size);
        errno = len;
    }

    if (head_buf) {
        vfree(head_buf);
    }

    return errno;
}

int
__block_write(struct device* dev, void* buf, size_t offset, size_t len)
{
    struct block_dev* bdev = (struct block_dev*)dev->underlay;
    size_t bsize = bdev->blk_size, wr_block = offset / bsize + bdev->start_lba,
           r = offset % bsize, wr_size = 0;

    if (!(len = MIN(len, ((size_t)bdev->end_lba - wr_block + 1) * bsize))) {
        return 0;
    }

    struct vecbuf* vbuf = NULL;
    struct blkio_req* req;
    void* tmp_buf = NULL;

    if (r) {
        size_t wr_size = MIN(len, bsize - r);
        tmp_buf = vzalloc(bsize);
        vbuf_alloc(&vbuf, tmp_buf, bsize);

        memcpy(tmp_buf + r, buf, wr_size);
    }

    if ((len - wr_size)) {
        size_t llen = len - wr_size;
        assert_msg(!(llen % bsize), "misalign block write");
        vbuf_alloc(&vbuf, buf + wr_size, llen);
    }

    req = blkio_vwr(vbuf, wr_block, NULL, NULL, 0);

    int errno;
    if (!(errno = __block_commit(bdev->blkio, req, BLKIO_WAIT))) {
        errno = len;
    }

    if (tmp_buf) {
        vfree(tmp_buf);
    }

    return errno;
}

int
__block_read_page(struct device* dev, void* buf, size_t offset)
{
    struct vecbuf* vbuf = NULL;
    struct block_dev* bdev = (struct block_dev*)dev->underlay;

    u32_t lba = offset / bdev->blk_size + bdev->start_lba;
    u32_t rd_lba = MIN(lba + PAGE_SIZE / bdev->blk_size, bdev->end_lba);

    if (rd_lba <= lba) {
        return 0;
    }

    rd_lba -= lba;

    vbuf_alloc(&vbuf, buf, rd_lba * bdev->blk_size);

    struct blkio_req* req = blkio_vrd(vbuf, lba, NULL, NULL, 0);

    int errno;
    if (!(errno = __block_commit(bdev->blkio, req, BLKIO_WAIT))) {
        errno = rd_lba * bdev->blk_size;
    }

    return errno;
}

int
__block_write_page(struct device* dev, void* buf, size_t offset)
{
    struct vecbuf* vbuf = NULL;
    struct block_dev* bdev = (struct block_dev*)dev->underlay;

    u32_t lba = offset / bdev->blk_size + bdev->start_lba;
    u32_t wr_lba = MIN(lba + PAGE_SIZE / bdev->blk_size, bdev->end_lba);

    if (wr_lba <= lba) {
        return 0;
    }

    wr_lba -= lba;

    vbuf_alloc(&vbuf, buf, wr_lba * bdev->blk_size);

    struct blkio_req* req = blkio_vwr(vbuf, lba, NULL, NULL, 0);

    int errno;
    if (!(errno = __block_commit(bdev->blkio, req, BLKIO_WAIT))) {
        errno = wr_lba * bdev->blk_size;
    }

    return errno;
}

int
__block_rd_lb(struct block_dev* bdev, void* buf, u64_t start, size_t count)
{
    struct vecbuf* vbuf = NULL;
    vbuf_alloc(&vbuf, buf, bdev->blk_size * count);

    struct blkio_req* req = blkio_vrd(vbuf, start, NULL, NULL, 0);

    int errno;
    if (!(errno = __block_commit(bdev->blkio, req, BLKIO_WAIT))) {
        errno = count;
    }

    return errno;
}

int
__block_wr_lb(struct block_dev* bdev, void* buf, u64_t start, size_t count)
{
    struct vecbuf* vbuf = NULL;
    vbuf_alloc(&vbuf, buf, bdev->blk_size * count);

    struct blkio_req* req = blkio_vwr(vbuf, start, NULL, NULL, 0);

    int errno;
    if (!(errno = __block_commit(bdev->blkio, req, BLKIO_WAIT))) {
        errno = count;
    }

    return errno;
}

void*
block_alloc_buf(struct block_dev* bdev)
{
    return valloc(bdev->blk_size);
}

void
block_free_buf(struct block_dev* bdev, void* buf)
{
    return vfree(buf);
}

struct block_dev*
block_alloc_dev(const char* blk_id, void* driver, req_handler ioreq_handler)
{
    struct block_dev* bdev = cake_grab(lbd_pile);
    memset(bdev, 0, sizeof(struct block_dev));
    llist_init_head(&bdev->parts);
    strncpy(bdev->name, blk_id, PARTITION_NAME_SIZE);

    bdev->blkio = blkio_newctx(ioreq_handler);
    bdev->driver = driver;
    bdev->blkio->driver = driver;
    bdev->ops = (struct block_dev_ops){ .block_read = __block_rd_lb,
                                        .block_write = __block_wr_lb };

    return bdev;
}

int
block_mount(struct block_dev* bdev, devfs_exporter fs_export)
{
    int errno = 0;

    if (!__block_register(bdev)) {
        errno = BLOCK_EFULL;
        goto error;
    }

    errno = blkpart_probegpt(bdev->dev);
    if (errno < 0) {
        ERROR("Fail to parse partition table (%d)", errno);
    } else if (!errno) {
        // TODO try other PT parser...
    }

    struct twifs_node* dev_root = twifs_dir_node(blk_sysroot, bdev->bdev_id);
    blk_set_blkmapping(bdev, dev_root);
    fs_export(bdev, dev_root);

    return errno;

error:
    ERROR("Fail to mount block device: %s (%x)", bdev->name, -errno);
    return errno;
}

int
__block_register(struct block_dev* bdev)
{
    if (free_slot >= MAX_DEV) {
        return 0;
    }

    struct device* dev = device_allocvol(dev_meta(blk_parent_dev), bdev);
    dev->ops.write = __block_write;
    dev->ops.write_page = __block_write_page;
    dev->ops.read = __block_read;
    dev->ops.read_page = __block_read_page;

    bdev->dev = dev;

    register_device(dev, bdev->class, "sd%c", 'a' + free_slot);
    dev_registry[free_slot++] = bdev;

    strcpy(bdev->bdev_id, dev->name_val);

    return 1;
}

struct block_dev*
blk_mount_part(struct block_dev* bdev,
               const char* name,
               size_t index,
               u64_t start_lba,
               u64_t end_lba)
{
    struct block_dev* pbdev = cake_grab(lbd_pile);
    memcpy(pbdev, bdev, sizeof(*bdev));

    struct device* dev = device_allocvol(NULL, pbdev);
    dev->ops.write = __block_write;
    dev->ops.write_page = __block_write_page;
    dev->ops.read = __block_read;
    dev->ops.read_page = __block_read_page;

    pbdev->start_lba = start_lba;
    pbdev->end_lba = end_lba;
    pbdev->dev = dev;

    strcpy(pbdev->bdev_id, dev->name_val);
    if (name) {
        strncpy(pbdev->name, name, PARTITION_NAME_SIZE);
    }

    llist_append(&bdev->parts, &pbdev->parts);

    register_device(dev, pbdev->class, "%sp%d", bdev->bdev_id, index + 1);

    return pbdev;
}