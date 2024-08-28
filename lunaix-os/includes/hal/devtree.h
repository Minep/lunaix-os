#ifndef __LUNAIX_DEVTREE_H
#define __LUNAIX_DEVTREE_H

#include <lunaix/types.h>
#include <lunaix/ds/llist.h>
#include <lunaix/ds/hstr.h>
#include <lunaix/ds/hashtable.h>
#include <lunaix/boot_generic.h>

#define le(v) ((((v) >> 24) & 0x000000ff)  |\
               (((v) << 8)  & 0x00ff0000)  |\
               (((v) >> 8)  & 0x0000ff00)  |\
               (((v) << 24) & 0xff000000))

#define le64(v) (((u64_t)le(v & 0xffffffff) << 32) | le(v >> 32))

#define be(v) ((((v) << 24) & 0x000000ff)  |\
               (((v) >> 8)  & 0x00ff0000)  |\
               (((v) << 8)  & 0x0000ff00)  |\
               (((v) >> 24) & 0xff000000))

#define FDT_MAGIC       be(0xd00dfeed)
#define FDT_NOD_BEGIN   be(0x00000001)
#define FDT_NOD_END     be(0x00000002)
#define FDT_PROP        be(0x00000003)
#define FDT_NOP         be(0x00000004)
#define FDT_END         be(0x00000009)

#define STATUS_OK       0
#define STATUS_DISABLE  1
#define STATUS_RSVD     2
#define STATUS_FAIL     3


typedef unsigned int* dt_enc_t;
typedef unsigned int  dt_phnd_t;

#define PHND_NULL    ((dt_phnd_t)-1)

struct fdt_header {
    u32_t magic;
    u32_t totalsize;
    u32_t off_dt_struct;
    u32_t off_dt_strings;
    u32_t off_mem_rsvmap;
    u32_t version;
    u32_t last_comp_version;
    u32_t boot_cpuid_phys;
    u32_t size_dt_strings;
    u32_t size_dt_struct;
};

struct fdt_memrsvd_ent 
{
    u64_t addr;
    u64_t size;
} align(8);

struct fdt_token 
{
    u32_t token;
} compact align(4);

struct fdt_node_head
{
    struct fdt_token token;
    char name[0];
};

struct fdt_prop 
{
    struct fdt_token token;
    u32_t len;
    u32_t nameoff;
} compact align(4);

struct dt_prop_val
{
    struct {
        union
        {
            union {
                const char*  str_val;
                const char** str_lst;
            };
            ptr_t        ptr_val;
            
            union {
                dt_enc_t     encoded;
                dt_phnd_t    phandle;
            };
            u32_t        u32_val;

            u64_t        u64_val;
        };
        unsigned int size;
    };
};


struct dt_prop
{
    struct hlist_node   ht;
    struct hstr         key;
    struct dt_prop_val  val;
};

struct dt_node_base
{
    union {
        struct {
            unsigned char addr_c;
            unsigned char sz_c;
            unsigned char intr_c;
            unsigned char status;
        };
        unsigned int _std;
    };

    union {
        struct {
            bool dma_coherent  : 1;
            bool dma_ncoherent : 1;
            bool intr_controll : 1;
            unsigned int other : 29;
        };
        unsigned int    flags;
    };

    struct dt_node_base  *parent;
    struct llist_header   children;
    struct llist_header   siblings;
    struct llist_header   nodes;
    struct hlist_node     phnd_link;

    const char*           name;

    struct dt_prop_val    compat;
    const char*           model;
    dt_phnd_t             phandle;

    union {
        struct hbucket    other_props[0];
        struct hbucket    _op_bucket[8];
    };
};

struct dt_root
{
    struct dt_node_base base;

    const char*         serial;
    const char*         chassis;
};

struct dt_intr_prop;

struct dt_intr_node
{
    union {
        struct dt_intr_node *parent;
        dt_phnd_t parent_hnd;
    };

    struct {
        bool extended;
        union {
            struct dt_prop_val   arr;
            struct llist_header  values; 
        };
    } intr;

    struct dt_prop_val       intr_map;
    struct dt_prop_val       intr_map_mask;
};
#define DT_NODE(intr_node) \
        (container_of(intr_node, struct dt_node, intr))


struct dt_node
{
    struct dt_node_base base;
    struct dt_intr_node intr;
    
    struct dt_prop_val  reg;
    struct dt_prop_val  vreg;

    struct dt_prop_val  ranges;
    struct dt_prop_val  dma_ranges;
};


struct dt_intr_prop
{
    struct dt_intr_node *master;

    struct llist_header  props;
    struct dt_prop_val   val;
};

struct dt_prop_iter
{
    struct dt_prop_val     *prop;
    struct dt_node_base    *node;
    dt_enc_t                prop_loc;
    dt_enc_t                prop_loc_next;
    unsigned int            ent_sz;
};

struct dt_context
{
    union {
        ptr_t reloacted_dtb;
        struct fdt_header* fdt;
    };

    struct llist_header   nodes;
    struct dt_root       *root;
    struct hbucket        phnds_table[16];
    const char           *str_block;
};

struct fdt_iter
{
    union {
        struct fdt_token      *pos;
        struct fdt_prop       *prop;
        struct fdt_node_head  *node_head;
    };

    const char* str_block;
    int depth;
};

struct fdt_memrsvd_iter
{
    struct fdt_memrsvd_ent *block;
};

struct dt_node_iter
{
    struct dt_node_base* head;
    struct dt_node_base* matched;
    const char *name;
};

#define dtnode_child_foreach(node_base, pos, n)  \
        llist_for_each(pos, n, &(node_base)->children, siblings)

#define fdt_prop(tok) ((tok)->token == FDT_PROP)
#define fdt_node(tok) ((tok)->token == FDT_NOD_BEGIN)
#define fdt_node_end(tok) ((tok)->token == FDT_NOD_END)
#define fdt_nope(tok) ((tok)->token == FDT_NOP)

void 
fdt_itbegin(struct fdt_iter* fdti, struct fdt_header* fdt_hdr);

void 
fdt_itend(struct fdt_iter* fdti);

bool 
fdt_itnext(struct fdt_iter* fdti);

bool 
fdt_itnext_at(struct fdt_iter* fdti, int level);

void
fdt_memrsvd_itbegin(struct fdt_memrsvd_iter* rsvdi, 
                    struct fdt_header* fdt_hdr);

bool
fdt_memrsvd_itnext(struct fdt_memrsvd_iter* rsvdi);

void
fdt_memrsvd_itend(struct fdt_memrsvd_iter* rsvdi);


bool
dt_load(ptr_t dtb_dropoff);

struct dt_node*
dt_resolve_phandle(dt_phnd_t phandle);

struct dt_prop_val*
dt_getprop(struct dt_node* node, const char* name);

void
dt_begin_find(struct dt_node_iter* iter, 
              struct dt_node* node, const char* name);

bool
dt_find_next(struct dt_node_iter* iter,
             struct dt_node_base** matched);

static inline bool
dt_found_any(struct dt_node_iter* iter)
{
    return !!iter->matched;
}


static inline char*
fdtit_prop_key(struct fdt_iter* fdti)
{
    return &fdti->str_block[fdti->prop->nameoff];
}

static inline void
dt_decode(struct dt_prop_iter* dtpi, struct dt_node_base* node, 
                struct dt_prop_val* val, unsigned int ent_sz)
{
    *dtpi = (struct dt_prop_iter) {
        .prop = val,
        .node = node,
        .prop_loc = val->encoded,
        .prop_loc_next = val->encoded,
        .ent_sz = ent_sz
    };
}

#define dt_decode_reg(dtpi, node, field) \
            dt_decode(dtpi, &(node)->base, (node)->(field), \
                            (node)->base.sz_c + (node)->base.addr_c);

#define dt_decode_range(dtpi, node, field) \
            dt_decode(dtpi, &(node)->base, (node)->field, \
                            (node)->base.sz_c * 2 + (node)->base.addr_c);

static inline void
dt_decode_intrmap(struct dt_prop_iter* dtpi, 
                  struct dt_intr_node* intr_node)
{
    unsigned int size;
    struct dt_node* node;
    struct dt_node_base* base;
    
    node = DT_NODE(intr_node);
    base = &node->base;
    size = (base->addr_c + base->intr_c) * 2 + 1;

    dt_decode(dtpi, base, &intr_node->intr_map, size);
}

#define dtprop_off(dtpi) \
            (unsigned int)(\
                __ptr(dtpi->prop_loc_next) - __ptr(dtpi->prop->encoded) \
            )

#define dtprop_extract(dtpi, off) \
            ( (dt_enc_t) (&(dtpi)->prop_loc[(off)]) )

static inline bool
dtprop_next_n(struct dt_prop_iter* dtpi, int n)
{
    unsigned int off;

    dtpi->prop_loc = dtpi->prop_loc_next;
    dtpi->prop_loc_next += n;

    off = dtprop_off(dtpi);
    return off >= dtpi->prop->size;
}

static inline bool
dtprop_prev_n(struct dt_prop_iter* dtpi, int n)
{
    unsigned int off;

    off = dtprop_off(dtpi);
    if (!off || off > dtpi->prop->size) {
        return false;
    }

    dtpi->prop_loc = dtpi->prop_loc_next;
    dtpi->prop_loc_next -= n;

    return true;
}

static inline bool
dtprop_next(struct dt_prop_iter* dtpi)
{
    return dtprop_next_n(dtpi, dtpi->ent_sz);
}

static inline bool
dtprop_prev(struct dt_prop_iter* dtpi)
{
    return dtprop_prev_n(dtpi, dtpi->ent_sz);
}

static inline unsigned int
dtprop_to_u32(dt_enc_t enc_val)
{
    return le(*enc_val);
}

#define dtprop_to_phnd(enc_val) \
            (dt_phnd_t)dtprop_to_u32(enc_val)

static inline u64_t
dtprop_to_u64(dt_enc_t enc_val)
{
    return le64(*(u64_t*)enc_val);
}

static inline dt_enc_t
dtprop_reg_addr(struct dt_prop_iter* dtpi)
{
    return dtprop_extract(dtpi, 0);
}

static inline dt_enc_t
dtprop_reg_len(struct dt_prop_iter* dtpi)
{
    return dtprop_extract(dtpi, dtpi->node->addr_c);
}

static inline dt_enc_t
dtprop_range_childbus(struct dt_prop_iter* dtpi)
{
    return dtprop_extract(dtpi, 0);
}

static inline dt_enc_t
dtprop_range_parentbus(struct dt_prop_iter* dtpi)
{
    return dtprop_extract(dtpi, dtpi->node->addr_c);
}

static inline dt_enc_t
dtprop_range_len(struct dt_prop_iter* dtpi)
{
    return dtprop_extract(dtpi, dtpi->node->addr_c * 2);
}

static inline dt_enc_t
dtprop_intr_cuaddr(struct dt_prop_iter* dtpi)
{
    return dtprop_extract(dtpi, 0);
}

static inline dt_enc_t
dtprop_intr_cispec(struct dt_prop_iter* dtpi)
{
    return dtprop_extract(dtpi, dtpi->node->addr_c);
}

static inline struct dt_intr_node*
dtprop_intr_parent(struct dt_prop_iter* dtpi)
{
    unsigned off;
    struct dt_node* node;
    dt_enc_t enc_val;

    off = dtpi->node->addr_c + dtpi->node->intr_c;
    enc_val = dtprop_extract(dtpi, off);
    node = dt_resolve_phandle(dtprop_to_phnd(enc_val));
    
    return &node->intr;
}

static inline dt_enc_t
dtprop_intr_puaddr(struct dt_prop_iter* dtpi)
{
    unsigned off;

    off = dtpi->node->addr_c + dtpi->node->intr_c + 1;
    return dtprop_extract(dtpi, off);
}

static inline dt_enc_t
dtprop_intr_pispec(struct dt_prop_iter* dtpi)
{
    unsigned off;

    off = dtpi->node->addr_c * 2 + dtpi->node->intr_c + 1;
    return dtprop_extract(dtpi, off);
}

#endif /* __LUNAIX_DEVTREE_H */
