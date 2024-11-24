#ifndef __LUNAIX_DEVTREE_H
#define __LUNAIX_DEVTREE_H

#include <lunaix/types.h>
#include <lunaix/ds/llist.h>
#include <lunaix/ds/hstr.h>
#include <lunaix/ds/hashtable.h>
#include <lunaix/changeling.h>

#include <klibc/string.h>

#define le(v) ((((v) >> 24) & 0x000000ff)  |\
               (((v) << 8)  & 0x00ff0000)  |\
               (((v) >> 8)  & 0x0000ff00)  |\
               (((v) << 24) & 0xff000000))

#define le64(v) (((u64_t)le(v & 0xffffffff) << 32) | le(v >> 32))

#define be(v) ((((v) & 0x000000ff) << 24)  |\
               (((v) & 0x00ff0000) >> 8 )  |\
               (((v) & 0x0000ff00) << 8 )  |\
               (((v) & 0xff000000) >> 24))

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

struct dt_node_base;
struct dt_node_iter;
typedef bool (*node_predicate_t)(struct dt_node_iter*, struct dt_node_base*);


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
                const char*  str_lst;
            };
            ptr_t        ptr_val;
            
            union {
                dt_enc_t     encoded;
                dt_phnd_t    phandle;
            };
            
            union {
                u32_t        u32_val;
                u64_t        u64_val;
            }* uval;
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

struct dt_prop_table
{
    union {
        struct hbucket    other_props[0];
        struct hbucket    _op_bucket[8];
    };
};

struct dt_node_base
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

    struct dt_node_base  *parent;
    struct llist_header   nodes;

    struct dt_prop_val    compat;
    dt_phnd_t             phandle;

    struct dt_prop_table* props;

    morph_t* binded_dev;
};

struct dt_intr_prop;

struct dt_intr_mapkey
{
    unsigned int* val;
    unsigned int  size;
};

struct dt_intr_mapent
{
    struct llist_header ents;

    struct dt_intr_mapkey key;

    struct dt_node_base* parent;
    struct dt_prop_val parent_props;
};

struct dt_intr_map 
{
    struct dt_prop_val       raw;
    struct dt_prop_val       raw_mask;

    struct dt_intr_mapkey    key_mask;
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
            struct dt_prop_val   arr;
            struct llist_header  values; 
        };
    } intr;

    struct dt_intr_map* map;
};
#define INTR_TO_DTNODE(intr_node) \
        (container_of(intr_node, struct dt_node, intr))

#define BASE_TO_DTNODE(base_node) \
        (container_of(base_node, struct dt_node, base))

struct dt_node
{
    union {
        morph_t mobj;
        struct dt_node_base base;
    };
    
    struct dt_intr_node intr;
    
    struct dt_prop_val  reg;
    struct dt_prop_val  vreg;

    struct dt_prop_val  ranges;
    struct dt_prop_val  dma_ranges;
};
#define dt_parent(node) ((node)->base.parent)
#define dt_morpher       morphable_attrs(dt_node, mobj)
#define dt_mobj(node)   (&(node)->mobj)

struct dt_intr_prop
{
    struct dt_intr_node *master;

    struct llist_header  props;
    struct dt_prop_val   val;
};

struct dtpropi
{
    struct dt_prop_val     *prop;
    struct dt_node_base    *node;
    dt_enc_t                prop_loc;
    dt_enc_t                prop_loc_next;
};

struct dt_context
{
    union {
        ptr_t reloacted_dtb;
        struct fdt_header* fdt;
    };

    struct llist_header   nodes;
    struct dt_node       *root;
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

struct dt_node_iter
{
    struct dt_node_base* head;
    struct dt_node_base* matched;
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
    return &fdti->str_block[le(fdti->prop->nameoff)];
}


/****
 * DT Main Functions: General
 ****/

bool
dt_load(ptr_t dtb_dropoff);

struct dt_node*
dt_resolve_phandle(dt_phnd_t phandle);

struct dt_prop_val*
dt_getprop(struct dt_node_base* base, const char* name);

struct dt_prop_val*
dt_resolve_interrupt(struct dt_node* node);

void
dt_resolve_interrupt_map(struct dt_node* node);

static inline unsigned int
dt_addr_cells(struct dt_node_base* base)
{
    return base->parent ? base->parent->addr_c : base->addr_c;
}

static inline unsigned int
dt_size_cells(struct dt_node_base* base)
{
    return base->parent ? base->parent->sz_c : base->sz_c;
}


/****
 * DT Main Functions: Node-finder
 ****/

void
dt_begin_find_byname(struct dt_node_iter* iter, 
                     struct dt_node* node, const char* name);

void
dt_begin_find(struct dt_node_iter* iter, struct dt_node* node, 
              node_predicate_t pred, void* closure);

static inline void
dt_end_find(struct dt_node_iter* iter)
{
    // currently do nothing, keep only for semantic
}

bool
dt_find_next(struct dt_node_iter* iter,
             struct dt_node_base** matched);

static inline bool
dt_found_any(struct dt_node_iter* iter)
{
    return !!iter->matched;
}

struct dt_context*
dt_main_context();

static inline u32_t
dtp_u32(struct dt_prop_val* val)
{
    return le(val->uval->u32_val);
}

static inline u64_t
dtp_u64(struct dt_prop_val* val)
{
    return le64(val->uval->u64_val);
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
        struct dt_node* phandle;
        dt_enc_t composite;
    };
    struct dtprop_def* archetype;
};

struct dtpropx
{
    const struct dtprop_def* proplet;
    int proplet_len;
    int proplet_sz;

    struct dt_prop_val* raw;
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
dtpx_prepare_with(struct dtpropx* propx, struct dt_prop_val* prop,
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
            le(*(u32_t*)val->composite) : val->u32;
}

static inline u64_t
dtpx_xvalu64(struct dtprop_xval* val){
    return val->archetype->type == DTP_COMPX ?
            le64(*(u64_t*)val->composite) : val->u64;
}

/****
 * DT Main Functions: Prop Iterator, for variable-sized property
 ****/

static inline void
dtpi_init(struct dtpropi* dtpi, struct dt_node_base* node, 
          struct dt_prop_val* val)
{
    *dtpi = (struct dtpropi) {
        .prop = val,
        .node = node,
        .prop_loc = val->encoded,
        .prop_loc_next = val->encoded,
    };
}

#define dtpi_off(dtpi) \
            (unsigned int)(\
                __ptr(dtpi->prop_loc_next) - __ptr(dtpi->prop->encoded) \
            )

#define dtpi_get(dtpi, off) \
            ( (dt_enc_t) (&(dtpi)->prop_loc[(off)]) )

static inline bool
dtpi_nextn(struct dtpropi* dtpi, int n)
{
    unsigned int off;

    dtpi->prop_loc = dtpi->prop_loc_next;
    dtpi->prop_loc_next += n;

    off = dtpi_off(dtpi);
    return off >= dtpi->prop->size;
}

static inline bool
dtpi_prevn(struct dtpropi* dtpi, int n)
{
    unsigned int off;

    off = dtpi_off(dtpi);
    if (!off || off > dtpi->prop->size) {
        return false;
    }

    dtpi->prop_loc = dtpi->prop_loc_next;
    dtpi->prop_loc_next -= n;

    return true;
}

static inline bool
dtpi_next(struct dtpropi* dtpi)
{
    return dtpi_nextn(dtpi, 1);
}

static inline bool
dtpi_prev(struct dtpropi* dtpi)
{
    return dtpi_prevn(dtpi, 1);
}

static inline u32_t
dtpi_u32_at(struct dtpropi* dtpi, int index)
{
    return le(*dtpi_get(dtpi, index));
}

static inline struct dt_node*
dtpi_refnode_at(struct dtpropi* dtpi, int index)
{
    return dt_resolve_phandle(le(*dtpi_get(dtpi, index)));
}

static inline u32_t
dtpi_u64_at(struct dtpropi* dtpi, int index)
{
    return le64(*(ptr_t*)dtpi_get(dtpi, index));
}

#endif /* __LUNAIX_DEVTREE_H */
