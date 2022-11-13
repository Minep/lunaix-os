#include <lunaix/fs/iso9660.h>

int
isorr_parse_px(struct iso_drecache* cache, void* px_start)
{
    struct isorr_px* px = (struct isorr_px*)px_start;
    cache->fno = px->sn.le;
    // TODO read other file attributes
    return px->header.length;
}

time_t
isorr_tf_gettime(struct isorr_tf* tf, int* index, u32_t type)
{
    time_t t = 0;
    if ((tf->flags & type)) {
        if ((tf->flags & ISORR_TF_LONG_FORM)) {
            t =
              iso9660_dt2unix((struct iso_datetime*)(tf->times + 17 * *index));
        } else {
            t =
              iso9660_dt22unix((struct iso_datetime2*)(tf->times + 7 * *index));
        }
        *index = *index + 1;
    }
    return t;
}

int
isorr_parse_tf(struct iso_drecache* cache, void* tf_start)
{
    struct isorr_tf* tf = (struct isorr_tf*)tf_start;
    int i = 0;
    cache->ctime = isorr_tf_gettime(tf, &i, ISORR_TF_CTIME);
    cache->mtime = isorr_tf_gettime(tf, &i, ISORR_TF_MTIME);
    cache->atime = isorr_tf_gettime(tf, &i, ISORR_TF_ATIME);
    // TODO read other file attributes
    return tf->header.length;
}

int
isorr_parse_nm(struct iso_drecache* cache, void* nm_start)
{
    u32_t i = 0, adv = 0;
    struct isorr_nm* nm;

    do {
        nm = (struct isorr_nm*)(nm_start + adv);
        u32_t len_name = nm->header.length - sizeof(*nm);
        memcpy(cache->name_val + i, nm->name, len_name);
        i += len_name;
        adv += nm->header.length;
    } while ((nm->flags & ISORR_NM_CONT) && i < ISO9660_IDLEN - 1);

    cache->name_val[i] = 0;
    cache->name = HSTR(cache->name_val, i);
    hstr_rehash(&cache->name, HSTR_FULL_HASH);

    return adv;
}