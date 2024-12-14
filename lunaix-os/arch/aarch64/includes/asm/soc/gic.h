#ifndef __LUNAIX_GIC_H
#define __LUNAIX_GIC_H

#include <lunaix/types.h>
#include <lunaix/ds/bitmap.h>
#include <lunaix/ds/hashtable.h>

#include <hal/devtree.h>

#include <asm/aa64_gic.h>
#include <asm-generic/isrm.h>

// nr of cpus, must be 1
#define NR_CPU      1
#define gic_bmp     PREP_BITMAP(gicreg_t, gic_intr, BMP_ORIENT_LSB)

#define INITID_SGI_BASE     0
#define INITID_SGI_END      15

#define INITID_PPI_BASE     16
#define INITID_PPI_END      31

#define INITID_SPI_BASE     32
#define INITID_SPI_END      1019

#define INITID_SPEC_BASE    1020
#define INITID_SPEC_END     1023

#define INITID_ePPI_BASE    1056
#define INITID_ePPI_END     1119

#define INITID_eSPI_BASE    4096
#define INITID_eSPI_END     5119

#define INITID_LPI_BASE     8192

#define INITID_SPI_NR       (INITID_SPEC_BASE - INITID_SPI_BASE)

enum gic_int_type
{
    GIC_IDL,
    GIC_LPI,
    GIC_SPI,
    GIC_PPI,
    GIC_SGI,
    GIC_RSV
} compact;

enum gic_tri_type
{
    GIC_TRIG_EDGE,
    GIC_TRIG_LEVEL
} compact;

enum gic_grp_type
{
    GIC_G0,
    GIC_G1S,
    GIC_G1NS
} compact;

DECLARE_BITMAP(gic_bmp);

struct gic_int_param
{
    enum gic_int_type class;
    enum gic_tri_type trigger;
    enum gic_grp_type group;
    unsigned int priority;
    unsigned int rel_intid;
    int cpu_id;
    bool as_nmi;
    bool ext_range;
};

struct gic_intcfg {
    enum gic_int_type class;
    enum gic_tri_type trigger;
    enum gic_grp_type group;
    bool as_nmi;
};

struct gic_idomain
{
    DECLARE_HASHTABLE(recs, 32);
    BITMAP(gic_bmp) ivmap;
    unsigned int base;
    bool extended;
};

struct gic_interrupt
{
    struct hlist_node node;

    struct gic_idomain* domain;
    
    unsigned int intid;
    struct gic_intcfg config;
    
    isr_cb handler;
    void* payload;
};

struct gic_distributor
{
    BITMAP(gic_bmp) group;
    BITMAP(gic_bmp) grpmod;
    BITMAP(gic_bmp) enable;
    BITMAP(gic_bmp) disable;
    BITMAP(gic_bmp) icfg;
    BITMAP(gic_bmp) nmi;
};

struct gic_rd
{
    gicreg_t base[FRAME_LEN];
    gicreg_t sgi_base[FRAME_LEN];
} compact align(4);

struct gic_cpuif
{
    gicreg_t base[FRAME_LEN];
} compact align(4);

#define gic_reg64(base, index) (*(gicreg64_t*)(&base[index]))
#define gic_regptr(base, index) (__ptr(&base[index]))

struct gic_pe
{
    struct gic_interrupt* active;
    reg_t iar_val;
    unsigned int priority;

    struct gic_rd* _rd;
    struct gic_cpuif* _if;

    struct gic_distributor rdist;

    struct {
        struct gic_idomain* local_ints;
        struct gic_idomain* eppi;
    } idomain;

    struct {
        unsigned int affinity;
        unsigned int ppi_nr;
        bool eppi_ready;
    };
};

struct gic_its_regs
{
    gicreg_t base[FRAME_LEN];        // control regs
    gicreg_t trn_space[FRAME_LEN];   // translation space
} compact align(4);

struct gic_its_regs_v41
{
    gicreg_t base[FRAME_LEN];        // control regs
    gicreg_t trn_space[FRAME_LEN];   // translation space
    gicreg_t vsgi_space[FRAME_LEN];  // vSGI space (v4.1+)
} compact align(4);

struct gic_its_cmdqeue
{
    gicreg64_t base;
    gicreg_t   wr_ptr;
    gicreg_t   rd_ptr;  
} compact align(4);

struct gic_its_cmd
{
    gicreg64_t dw[4];
};

struct gic_its_devmap
{
    struct hlist_node node;
    unsigned int devid;
    unsigned int next_evtid;
};

struct gic_its
{
    struct llist_header its;

    struct {
        unsigned int nr_devid;
        unsigned int nr_evtid;
        unsigned int nr_cid;
        unsigned int sz_itte;

        bool pta;
        bool ext_cids;
    };

    union {
        struct gic_its_regs* reg;
        struct gic_its_regs_v41* reg_v41;
        ptr_t  reg_ptr;
    };

    struct {
        union {
            struct gic_its_cmdqeue *cmd_queue;
            ptr_t  cmd_queue_ptr;
        };
        
        union {
            struct gic_its_cmd *cmds;
            ptr_t cmds_ptr;
        };

        unsigned int max_cmd;
    };

    union {
        struct {
            gicreg_t base[8];
        } *tables;
        ptr_t  table_ptr;
    };

    DECLARE_HASHTABLE(devmaps, 8);
};

typedef unsigned char lpi_entry_t;

struct arm_gic
{
    unsigned int max_intid;
    struct gic_pe pes[NR_CPU];

    struct {
        unsigned int lpi_nr;
        unsigned int spi_nr;
        unsigned int espi_nr;
        bool lpi_ready;
        bool nmi_ready;
        bool has_espi;
        bool msi_via_spi;
    };

    struct {
        gicreg_t* dist_base;
    } mmrs;

    struct llist_header its;

    struct {
        ptr_t prop_pa;
        lpi_entry_t* prop_table;
        
        ptr_t pend;
        BITMAP(gic_bmp) pendings;
    } lpi_tables;

    struct gic_distributor dist;
    struct gic_distributor dist_e;

    struct {
        struct gic_idomain* lpi;
        struct gic_idomain* spi;
        struct gic_idomain* espi;
    } idomain;
};

void
gic_create_from_dt(struct arm_gic* gic, struct dtn* node);

unsigned int;
gic_decode_specifier(struct gic_int_param* param, 
                     struct dtp_val* val, int nr_cells);

struct gic_its*
gic_its_create(struct arm_gic* gic, ptr_t regs);

void
gic_configure_its(struct arm_gic* gic);

struct gic_interrupt*
gic_install_int(struct gic_int_param* param, isr_cb handler, bool alloc);

unsigned int
gic_its_map_lpi(struct gic_its* its, 
                unsigned int devid, unsigned int lpid);

#endif /* __LUNAIX_GIC_H */
