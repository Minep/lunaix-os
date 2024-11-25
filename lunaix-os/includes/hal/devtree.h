#ifndef __LUNAIX_DEVTREE_H
#define __LUNAIX_DEVTREE_H

#include <lunaix/types.h>
#include <lunaix/ds/llist.h>
#include <lunaix/ds/hstr.h>
#include <lunaix/ds/hashtable.h>
#include <lunaix/changeling.h>

#include <klibc/string.h>

#define FDT_MAGIC       0xd00dfeedU
#define FDT_NOD_BEGIN   0x00000001U
#define FDT_NOD_END     0x00000002U
#define FDT_PROP        0x00000003U
#define FDT_NOP         0x00000004U
#define FDT_END         0x00000009U

#define STATUS_OK       0
#define STATUS_DISABLE  1
#define STATUS_RSVD     2
#define STATUS_FAIL     3


typedef unsigned int*     dt_enc_t;
typedef unsigned int      dt_phnd_t;

struct dtn_base;
struct dtn_iter;
typedef bool (*node_predicate_t)(struct dtn_iter*, struct dtn_base*);


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
} _be;

struct fdt_memrsvd_ent 
{
    u64_t addr;
    u64_t size;
} _be align(8);

struct fdt_token 
{
    u32_t token;
} _be compact align(4);

struct fdt_node_head
{
    struct fdt_token token;
    char name[0];
} _be;

struct fdt_prop
{
    struct fdt_token token;
    u32_t len;
    u32_t nameoff;

    u32_t val[0];
    char  val_str[0];
} _be compact align(4);

union dtp_baseval
{
    u32_t        u32_val;
    u64_t        u64_val;
    dt_phnd_t    phandle;
    u32_t        raw[0];
} _be;

struct dtp_val
{
    struct {
        union
        {
            union {
                const char*  str_val;
                const char*  str_lst;
            };
            ptr_t        ptr_val;
            dt_enc_t     encoded;
            u32_t _be    *raw;
            
            union dtp_baseval* uval;
        };
        unsigned int size;
    };
};


struct dtp
{
    struct hlist_node   ht;
    struct hstr         key;
    struct dtp_val  val;
};

struct dtp_table
{
    union {
        struct hbucket    other_props[0];
        struct hbucket    _op_bucket[8];
    };
};

struct dtn_base
{
    morph_t mobj;

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
            bool intr_neuxs    : 1;
        };
        unsigned int    flags;
    };

    struct dtn_base  *parent;
    struct llist_header   nodes;

    struct dtp_val    compat;
    dt_phnd_t             phandle;

    struct dtp_table* props;

    morph_t* binded_dev;
};

struct dt_intr_prop;

struct dt_speckey
{
    union {
        union dtp_baseval *bval;
        unsigned int*      val;
    };
    unsigned int      size;
};

struct dt_intr_mapent
{
    struct llist_header ents;

    struct dt_speckey key;

    struct dtn_base* parent;
    struct dtp_val parent_props;
};

struct dt_intr_map 
{
    struct dtp_val       raw;
    struct dtp_val       raw_mask;

    struct dt_speckey    key_mask;
    struct llist_header      mapent;

    bool resolved;
};

struct dt_intr_node
{
    union {
        struct dt_intr_node *parent;
        dt_phnd_t parent_hnd;
    };

    struct {
        union {
            struct {
                bool extended : 1;
                bool valid : 1;
            };
            int flags;
        };

        union {
            struct dtp_val   arr;
            struct llist_header  values; 
        };
    } intr;

    struct dt_intr_map* map;
};
#define INTR_TO_DTNODE(intr_node) \
        (container_of(intr_node, struct dtn, intr))

#define BASE_TO_DTNODE(base_node) \
        (container_of(base_node, struct dtn, base))

struct dtn
{
    union {
        morph_t mobj;
        struct dtn_base base;
    };
    
    struct dt_intr_node intr;
    
    struct dtp_val  reg;
    struct dtp_val  vreg;

    struct dtp_val  ranges;
    struct dtp_val  dma_ranges;
};
#define dt_parent(node) ((node)->base.parent)
#define dt_morpher       morphable_attrs(dtn, mobj)
#define dt_mobj(node)   (&(node)->mobj)

struct dt_intr_prop
{
    struct dt_intr_node *master;

    struct llist_header  props;
    struct dtp_val   val;
};

struct dt_context
{
    union {
        ptr_t reloacted_dtb;
        struct fdt_header* fdt;
    };

    struct llist_header   nodes;
    struct dtn       *root;
    struct hbucket        phnds_table[16];
    const char           *str_block;
};

enum fdt_state {
    FDT_STATE_START,
    FDT_STATE_NODE,
    FDT_STATE_NODE_EXIT,
    FDT_STATE_PROP,
    FDT_STATE_END,
};

struct fdt_iter
{
    union {
        struct fdt_token      *pos;
        struct fdt_prop       *prop;
        struct fdt_node_head  *node_head;
        struct {
            struct fdt_token token;
            char extra_data[0];
        } *detail;
    };

    struct fdt_token* next;
    
    const char* str_block;
    int depth;
    enum fdt_state state;
    bool invalid;
};

struct fdt_memrsvd_iter
{
    struct fdt_memrsvd_ent *block;
};

struct dtn_iter
{
    struct dtn_base* head;
    struct dtn_base* matched;
    void* closure;
    node_predicate_t pred;
};



/****
 * FDT Related
 ****/

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

static inline char*
fdtit_prop_key(struct fdt_iter* fdti)
{
    return &fdti->str_block[fdti->prop->nameoff];
}


/****
 * DT Main Functions: General
 ****/

bool
dt_load(ptr_t dtb_dropoff);

struct dtn*
dt_resolve_phandle(dt_phnd_t phandle);

struct dtp_val*
dt_getprop(struct dtn_base* base, const char* name);

struct dtp_val*
dt_resolve_interrupt(struct dtn* node);

void
dt_resolve_interrupt_map(struct dtn* node);

void
dt_speckey_mask(struct dt_speckey* k, struct dt_speckey* mask);


/****
 * DT Main Functions: Node-finder
 ****/

void
dt_begin_find_byname(struct dtn_iter* iter, 
                     struct dtn* node, const char* name);

void
dt_begin_find(struct dtn_iter* iter, struct dtn* node, 
              node_predicate_t pred, void* closure);

static inline void
dt_end_find(struct dtn_iter* iter)
{
    // currently do nothing, keep only for semantic
}

bool
dt_find_next(struct dtn_iter* iter,
             struct dtn_base** matched);

static inline bool
dt_found_any(struct dtn_iter* iter)
{
    return !!iter->matched;
}

struct dt_context*
dt_main_context();

static inline u32_t
dtp_u32(struct dtp_val* val)
{
    return val->uval->u32_val;
}

static inline u64_t
dtp_u64(struct dtp_val* val)
{
    return val->uval->u64_val;
}

static inline void
dtp_speckey(struct dt_speckey* key, struct dtp_val* prop)
{
    key->size = prop->size / sizeof(u32_t);
    key->val  = prop->encoded;
}

/****
 * DT Main Functions: Prop Extractor, for fixed size properties
 ****/

enum dtprop_types
{
    DTP_END = 0,
    DTP_U32,
    DTP_U64,
    DTP_PHANDLE,
    DTP_COMPX,
};

struct dtprop_def
{
    unsigned int cell;
    unsigned int acc_sz;
    enum dtprop_types type;
};

typedef struct dtprop_def dt_proplet[];

struct dtprop_xval
{
    union {
        u32_t u32;
        ptr_t u64;
        struct dtn* phandle;
        union {
            dt_enc_t composite;
            union dtp_baseval* cval;
        };
    };
    struct dtprop_def* archetype;
};

struct dtpropx
{
    const struct dtprop_def* proplet;
    int proplet_len;
    int proplet_sz;

    struct dtp_val* raw;
    off_t row_loc;
};

#define dtprop_u32              (struct dtprop_def){ 1, 0, DTP_U32 }
#define dtprop_u64              (struct dtprop_def){ 2, 0, DTP_U64 }
#define dtprop_handle           (struct dtprop_def){ 1, 0, DTP_PHANDLE }
#define dtprop_compx(cell)      (struct dtprop_def){ cell, 0, DTP_COMPX }
#define dtprop_end              (struct dtprop_def){ 0, 0, DTP_END }
#define dtprop_(type, cell)     (struct dtprop_def){ cell, 0, type }

#define dtprop_reglike(base)                    \
    ({                                          \
        dt_proplet p = {                        \
            dtprop_compx(base->addr_c),         \
            dtprop_compx(base->sz_c),           \
            dtprop_end                          \
        };                                      \
        dt_proplet;                             \
    })

#define dtprop_rangelike(node)                  \
    ({                                          \
        dt_proplet p = {                        \
            dtprop_compx(base->addr_c),         \
            dtprop_compx(base->parent->addr_c), \
            dtprop_compx(base->sz_c),           \
            dtprop_end                          \
        };                                      \
        dt_proplet;                             \
    })

#define dtprop_strlst_foreach(pos, prop)    \
        for (pos = (prop)->str_lst; \
             pos <= &(prop)->str_lst[(prop)->size - 1]; \
             pos = &pos[strlen(pos) + 1])

void
dtpx_compile_proplet(struct dtprop_def* proplet);

void
dtpx_prepare_with(struct dtpropx* propx, struct dtp_val* prop,
                  struct dtprop_def* proplet);

#define dtproplet_compile(proplet)   \
        dtpx_compile_proplet(proplet, \
                          sizeof(proplet) / sizeof(struct dtprop_def))

bool
dtpx_goto_row(struct dtpropx*, int row);

bool
dtpx_next_row(struct dtpropx*);

bool
dtpx_extract_at(struct dtpropx*, struct dtprop_xval*, int col);

bool
dtpx_extract_loc(struct dtpropx*, struct dtprop_xval*, 
                 int row, int col);

bool
dtpx_extract_row(struct dtpropx*, struct dtprop_xval*, int len);

static inline u32_t
dtpx_xvalu32(struct dtprop_xval* val){
    return val->archetype->type == DTP_COMPX ?
            val->cval->u32_val : val->u32;
}

static inline u64_t
dtpx_xvalu64(struct dtprop_xval* val){
    return val->archetype->type == DTP_COMPX ?
            val->cval->u64_val : val->u64;
}

/****
 * DT Main Functions: Prop Iterator, for variable-sized property
 ****/

struct dtpropi
{
    struct dtp_val     *prop;
    off_t loc;
};

static inline void
dtpi_init(struct dtpropi* dtpi, struct dtp_val* val)
{
    *dtpi = (struct dtpropi) {
        .prop = val,
        .loc = 0
    };
}

static inline bool
dtpi_has_next(struct dtpropi* dtpi)
{
    return dtpi->loc >= dtpi->prop->size / sizeof(u32_t);
}

static inline u32_t
dtpi_next_u32(struct dtpropi* dtpi) 
{
    union dtp_baseval* val;
    val = (union dtp_baseval*)&dtpi->prop->encoded[dtpi->loc++];
    return val->u32_val;
}

static inline u64_t
dtpi_next_u64(struct dtpropi* dtpi) 
{
    union dtp_baseval* val;
    off_t loc = dtpi->loc;
    dtpi->loc += 2;
    val = (union dtp_baseval*)&dtpi->prop->encoded[loc];
    
    return val->u64_val;
}

static inline struct dtn*
dtpi_next_hnd(struct dtpropi* dtpi) 
{
    u32_t phandle;
    phandle = dtpi_next_u32(dtpi);
    return dt_resolve_phandle(phandle);
}

static inline bool
dtpi_next_val(struct dtpropi* dtpi, struct dtp_val* val, int cells) 
{
    if (!dtpi_has_next(dtpi)) {
        return false;
    }

    off_t loc = dtpi->loc;
    val->encoded = &dtpi->prop->encoded[loc];
    val->size = cells * sizeof(u32_t);

    dtpi->loc += cells;
    return true;
}

#endif /* __LUNAIX_DEVTREE_H */
