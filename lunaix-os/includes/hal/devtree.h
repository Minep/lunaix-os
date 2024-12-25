#ifndef __LUNAIX_DEVTREE_H
#define __LUNAIX_DEVTREE_H

#ifdef CONFIG_USE_DEVICETREE
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

#define PHND_NULL    ((dt_phnd_t)-1)

/////////////////////////////////
///     DT Primitives
/////////////////////////////////

typedef unsigned int*     dt_enc_t;
typedef unsigned int      dt_phnd_t;

struct dtn_base;
struct dtn_iter;
typedef bool (*node_predicate_t)(struct dtn_iter*, struct dtn_base*);

union dtp_baseval
{
    u32_t        u32_val;
    u64_t        u64_val;
    dt_phnd_t    phandle;
    u32_t        raw[0];
} _be;

struct dtp_val
{
    union
    {
        ptr_t        ptr_val;
        dt_enc_t     encoded;
        
        union dtp_baseval* ref;
        union {
            const char*  str_val;
            const char*  str_lst;
        };
    };
    unsigned int size;
};

struct dtpropi
{
    struct dtp_val     prop;
    off_t loc;
};

/////////////////////////////////
///     FDT Constructs
/////////////////////////////////

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

struct fdt_prop
{
    struct fdt_token token;
    u32_t len;
    u32_t nameoff;

    union {
        u32_t val[0];
        char  val_str[0];
    } _be;
} _be compact align(4);

struct fdt_loc
{
    union {
        struct fdt_token        *token;
        struct fdt_prop         *prop;
        struct fdt_memrsvd_ent  *rsvd_ent;
        ptr_t                    ptr;
        struct {
            struct fdt_token token;
            char name[0];
        } *node;
    };
};
typedef struct fdt_loc fdt_loc_t;


enum fdt_state {
    FDT_STATE_START,
    FDT_STATE_NODE,
    FDT_STATE_NODE_EXIT,
    FDT_STATE_PROP,
    FDT_STATE_END,
};

enum fdtit_mode {
    FDT_RSVDMEM,
    FDT_STRUCT,
};

enum fdt_mem_type {
    FDT_MEM_RSVD,
    FDT_MEM_RSVD_DYNAMIC,
    FDT_MEM_FREE
};

struct fdt_rsvdmem_attrs
{
    size_t total_size;
    ptr_t alignment;
    
    union {
        struct {
            bool nomap    : 1;
            bool reusable : 1;
        };
        int flags;
    };
};

struct dt_memory_node
{
    ptr_t base;
    ptr_t size;
    enum fdt_mem_type type;

    struct fdt_rsvdmem_attrs dyn_alloc_attr;
};

struct fdt_rsvd_mem
{
    u64_t base;
    u64_t size;
} _be align(8);

struct fdt_blob
{
    union {
        struct fdt_header* header;
        ptr_t fdt_base;
    };

    fdt_loc_t root;
    union {
        const char* str_block;
        ptr_t str_block_base;
    };


    union {
        struct fdt_rsvd_mem* plat_rsvd;
        ptr_t plat_rsvd_base;
    };
};

struct fdt_memscan
{
    fdt_loc_t loc;
    fdt_loc_t found;
    
    struct dtpropi regit;

    u32_t root_addr_c;
    u32_t root_size_c;
    enum fdt_mem_type node_type;

    struct fdt_rsvdmem_attrs node_attr;
};

struct fdt_memrsvd_iter
{
    struct fdt_memrsvd_ent *block;
};


/////////////////////////////////
///     DT Construct
/////////////////////////////////

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
        };
        unsigned int    flags;
    };

    struct dtn_base         *parent;
    struct llist_header      nodes;

    struct dtp_val           compat;
    dt_phnd_t                phandle;

    struct dtp_table        *props;

    morph_t                 *binded_obj;
};

struct dtspec_key
{
    union {
        union dtp_baseval *bval;
        unsigned int*      val;
    };
    unsigned int      size;
};

struct dtspec_mapent
{
    struct llist_header ents;
    const struct dtspec_key child_spec;
    
    struct dtn* parent;
    const struct dtspec_key parent_spec;
};

struct dtspec_map
{
    const struct dtspec_key mask;
    const struct dtspec_key pass_thru;

    struct llist_header ents;
};

struct dtspec_intr
{
    struct dtn *domain;

    struct llist_header  ispecs;
    struct dtp_val   val;
};

struct dtn_intr
{
    union {
        struct dtn *parent;
        dt_phnd_t parent_hnd;
    };

    union {
        struct {
            bool extended : 1;
            bool valid : 1;
        };
        int flags;
    };

    union {
        struct dtp_val       raw_ispecs;
        struct llist_header  ext_ispecs; 
    };
    
    int nr_intrs;

    struct dtspec_map* map;
};

struct dtn
{
    union {
        morph_t mobj;
        struct dtn_base base;
    };
    
    struct dtn_intr intr;
    
    struct dtp_val  reg;

    struct dtp_val  ranges;
    struct dtp_val  dma_ranges;
};
#define dt_parent(node)  ((node)->base.parent)
#define dt_morpher       morphable_attrs(dtn, mobj)
#define dt_mobj(node)    (&(node)->mobj)
#define dt_name(node)    morpher_name(dt_mobj(node))
#define dtn_from(base_node) \
        (container_of(base_node, struct dtn, base))

struct dtn_iter
{
    struct dtn_base* head;
    struct dtn_base* matched;
    void* closure;
    node_predicate_t pred;
};

struct dt_context
{
    struct fdt_blob         fdt;

    struct llist_header     nodes;
    struct dtn             *root;
    struct hbucket          phnds_table[16];
    const char             *str_block;
};


/////////////////////////////////
///     FDT Methods
/////////////////////////////////

#define fdt_prop(tok) ((tok)->token == FDT_PROP)
#define fdt_node(tok) ((tok)->token == FDT_NOD_BEGIN)
#define fdt_nope(tok) ((tok)->token == FDT_NOP)
#define fdt_eof(tok)  ((tok)->token == FDT_END)
#define fdt_node_end(tok) \
    ((tok)->token == FDT_NOD_END || (tok)->token == FDT_END)

void
fdt_load(struct fdt_blob* fdt, ptr_t base);

fdt_loc_t
fdt_next_token(fdt_loc_t loc, int* depth);

bool
fdt_next_sibling(fdt_loc_t loc, fdt_loc_t* loc_out);

bool
fdt_next_boot_rsvdmem(struct fdt_blob*, fdt_loc_t*, struct dt_memory_node*);

fdt_loc_t
fdt_descend_into(fdt_loc_t loc);

static inline fdt_loc_t
fdt_ascend_from(fdt_loc_t loc) 
{
    while (fdt_next_sibling(loc, &loc));

    loc.token++;
    return loc;
}

bool
fdt_memscan_begin(struct fdt_memscan*, const struct fdt_blob*);

bool
fdt_memscan_nextnode(struct fdt_memscan*, struct fdt_blob*);

bool
fdt_memscan_nextrange(struct fdt_memscan*, struct dt_memory_node*);

bool
fdt_find_prop(const struct fdt_blob*, fdt_loc_t, 
              const char*, struct dtp_val*);

static inline char*
fdt_prop_key(struct fdt_blob* fdt, fdt_loc_t loc)
{
    return &fdt->str_block[loc.prop->nameoff];
}


/////////////////////////////////
///     DT General Methods
/////////////////////////////////

bool
dt_load(ptr_t dtb_dropoff);

struct dtn*
dt_resolve_phandle(dt_phnd_t phandle);

struct dtp_val*
dt_getprop(struct dtn_base* base, const char* name);

void
dt_resolve_interrupt_map(struct dtn* node);

struct dtn* 
dt_interrupt_at(struct dtn* node, int idx, struct dtp_val* int_spec);

static inline void
dtp_val_set(struct dtp_val* val, dt_enc_t raw, unsigned cells)
{
    val->encoded = raw;
    val->size = cells * sizeof(u32_t);
}

static inline void
dtn_bind_object(struct dtn* node, morph_t* mobj)
{
    node->base.binded_obj = changeling_ref(mobj);
}


//////////////////////////////////////
///     DT Methods: Specifier Map
//////////////////////////////////////


struct dtspec_create_ops
{
    int (*child_keysz)(struct dtn*);
    int (*parent_keysz)(struct dtn*);
    struct dtp_val* (*get_mask)(struct dtn*);
    struct dtp_val* (*get_passthru)(struct dtn*);
};

struct dtspec_map*
dtspec_create(struct dtn* node, struct dtp_val* map,
              const struct dtspec_create_ops* ops);

struct dtspec_mapent*
dtspec_lookup(struct dtspec_map*, struct dtspec_key*);

void
dtspec_applymask(struct dtspec_map*, struct dtspec_key*);

void
dtspec_free(struct dtspec_map*);

void
dtspec_cpykey(struct dtspec_key* dest, struct dtspec_key* src);

void
dtspec_freekey(struct dtspec_key* key);

static inline bool
dtspec_nullkey(struct dtspec_key* key)
{
    return !key || !key->size;
}


//////////////////////////////////////
///     DT Methods: Node query
//////////////////////////////////////

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
    return val->ref->u32_val;
}

static inline u64_t
dtp_u64(struct dtp_val* val)
{
    return val->ref->u64_val;
}

static inline void
dtp_speckey(struct dtspec_key* key, struct dtp_val* prop)
{
    key->size = prop->size / sizeof(u32_t);
    key->val  = prop->encoded;
}


//////////////////////////////////////
///     DT Prop Extractor
//////////////////////////////////////

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
            dtprop_compx((base)->addr_c),       \
            dtprop_compx((base)->sz_c),         \
            dtprop_end                          \
        };                                      \
        p;                             \
    })

#define dtprop_rangelike(node)                  \
    ({                                          \
        dt_proplet p = {                        \
            dtprop_compx(base->addr_c),         \
            dtprop_compx(base->parent->addr_c), \
            dtprop_compx(base->sz_c),           \
            dtprop_end                          \
        };                                      \
        p;                             \
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


//////////////////////////////////////
///     DT Prop Iterator
//////////////////////////////////////

static inline void
dtpi_init(struct dtpropi* dtpi, struct dtp_val* val)
{
    *dtpi = (struct dtpropi) {
        .prop = *val,
        .loc = 0
    };
}

static inline void
dtpi_init_empty(struct dtpropi* dtpi)
{
    *dtpi = (struct dtpropi) {
        .prop = { {0}, 0 },
        .loc = 0
    };
}

static inline bool
dtpi_is_empty(struct dtpropi* dtpi)
{
    return !dtpi->prop.size;
}

static inline bool
dtpi_has_next(struct dtpropi* dtpi)
{
    return dtpi->loc < dtpi->prop.size / sizeof(u32_t);
}

static inline u64_t
dtpi_next_integer(struct dtpropi* dtpi, int int_cells) 
{
    union dtp_baseval* val;
    off_t loc = dtpi->loc;
    dtpi->loc += int_cells;
    val = (union dtp_baseval*)&dtpi->prop.encoded[loc];
    
    return int_cells == 1 ? val->u32_val : val->u64_val;
}

static inline u64_t
dtpi_next_u64(struct dtpropi* dtpi) 
{
    return dtpi_next_integer(dtpi, 2);
}

static inline u32_t
dtpi_next_u32(struct dtpropi* dtpi) 
{
    return (u32_t)dtpi_next_integer(dtpi, 1);
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
    dtp_val_set(val, &dtpi->prop.encoded[loc], cells);

    dtpi->loc += cells;
    return true;
}

#endif
#endif /* __LUNAIX_DEVTREE_H */
