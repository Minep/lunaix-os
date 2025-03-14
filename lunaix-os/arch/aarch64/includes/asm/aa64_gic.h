#ifndef __LUNAIX_AA64_GIC_H
#define __LUNAIX_AA64_GIC_H

#include <lunaix/bits.h>
#include "aa64_msrs.h"
#include "hart.h"

#define FRAME_SIZE  0x10000

typedef unsigned int gicreg_t;
typedef unsigned long gicreg64_t;
#define FRAME_LEN   (FRAME_SIZE / sizeof(gicreg_t))
#define REG_INDEX(addr)     ((addr) / sizeof(gicreg_t))

#define ICC_CTLR_EL1        __sr_encode(3, 0, 12, 12, 4)
#define ICC_BPR0_EL1        __sr_encode(3, 0, 12,  8, 3)
#define ICC_PMR_EL1         __sr_encode(3, 0,  4,  6, 0)

#define ICC_SRE_EL1         __sr_encode(3, 0, 12, 12, 5)
#define ICC_SRE_EL2         __sr_encode(3, 4, 12,  9, 5)
#define ICC_SRE_EL3         __sr_encode(3, 6, 12, 12, 5)

#define ICC_IGRPEN0_EL1     __sr_encode(3, 0, 12, 12, 6)
#define ICC_IGRPEN1_EL1     __sr_encode(3, 0, 12, 12, 7)

#define ICC_IAR1_EL1        __sr_encode(3, 0, 12, 12, 0)
#define ICC_NMIAR1_EL1      __sr_encode(3, 0, 12,  9, 5)
#define ICC_EOIR1_EL1       __sr_encode(3, 0, 12, 12, 1)
#define ICC_DIR_EL1         __sr_encode(3, 0, 12, 11, 1)

#define INTID_ACKED_S        1020
#define INTID_ACKED_NS       1021
#define INTID_IAR1_NMI       1022
#define INTID_NOTHING        1023
#define check_special_intid(intid)  \
        ((intid) >= INTID_ACKED_S && (intid) <= INTID_NOTHING)

#define LPI_PRIORITY         BITFIELD(7, 2)
#define LPI_EN               1UL

#define ICC_SRE_SRE          BITFLAG(0)
#define ICC_SRE_DFB          BITFLAG(1)
#define ICC_SRE_DIB          BITFLAG(2)

#define ICC_CTRL_EXTRAN      BITFLAG(19)
#define ICC_CTRL_IDbits      BITFIELD(13, 11)
#define ICC_CTRL_PRIbits     BITFIELD(10, 8)
#define ICC_CTRL_PMHE        BITFLAG(6)
#define ICC_CTRL_EOImode     BITFLAG(1)
#define ICC_CTRL_CBPR        BITFLAG(0)

#define ICC_IGRPEN_ENABLE    BITFLAG(0)

#define GICD_CTLR            REG_INDEX(0x0000)
#define GICD_TYPER           REG_INDEX(0x0004)
#define GICD_IIDR            REG_INDEX(0x0008)
#define GICD_SETSPI_NSR      REG_INDEX(0x0040)

#define GICD_IGROUPRn        REG_INDEX(0x0080)
#define GICD_ISENABLER       REG_INDEX(0x0100)
#define GICD_ICENABLER       REG_INDEX(0x0180)
#define GICD_IPRIORITYR      REG_INDEX(0x0400)
#define GICD_ICFGR           REG_INDEX(0x0C00)
#define GICD_IGRPMODRn       REG_INDEX(0x0D00)
#define GICD_INMIR           REG_INDEX(0x0F80)

#define GICR_CTLR            REG_INDEX(0x0000)
#define GICR_TYPER           REG_INDEX(0x0008)
#define GICR_PROPBASER       REG_INDEX(0x0070)
#define GICR_PENDBASER       REG_INDEX(0x0078)
#define GICR_SETLPIR         REG_INDEX(0x0040)

#define GITS_CTLR            REG_INDEX(0x0000)
#define GITS_TYPER           REG_INDEX(0x0004)
#define GITS_CBASER          REG_INDEX(0x0080)
#define GITS_BASER           REG_INDEX(0x0100)

#define GICD_CTLR_DS         BITFLAG(6)
#define GICD_CTLR_ARE_NS     BITFLAG(5)
#define GICD_CTLR_ARE_S      BITFLAG(4)
#define GICD_CTLR_G1SEN      BITFLAG(2)
#define GICD_CTLR_G1NSEN     BITFLAG(1)
#define GICD_CTLR_G0EN       BITFLAG(0)

#define GICD_TYPER_nESPI     BITFIELD(31, 27)
#define GICD_TYPER_No1N      BITFLAG(25)
#define GICD_TYPER_LPIS      BITFLAG(17)
#define GICD_TYPER_MBIS      BITFLAG(16)
#define GICD_TYPER_nLPI      BITFIELD(15, 11)
#define GICD_TYPER_NMI       BITFLAG(9)
#define GICD_TYPER_ESPI      BITFLAG(8)
#define GICD_TYPER_nSPI      BITFIELD(4, 0)
#define GICD_TYPER_IDbits    BITFIELD(23, 19)

#define GICR_TYPER_AffVal    BITFIELD(63, 32)
#define GICR_TYPER_PPInum    BITFIELD(31, 27)

#define GICR_TYPER_DirectLPI    BITFLAG(3)

#define GICR_BASER_PAddr     BITFIELD(51, 12)
#define GICR_BASER_Share     BITFIELD(11, 10)
#define GICR_PENDBASER_PTZ   BITFLAG(62)
#define GICR_PROPBASER_IDbits\
                             BITFIELD(4,  0)

#define GICR_CTLR_RWP        BITFLAG(31)
#define GICR_CTLR_EnLPI      BITFLAG(0)

#define GITS_CTLR_QS         BITFLAG(31)
#define GITS_CTLR_EN         BITFLAG(0)

#define GITS_TYPER_CIL       BITFLAG(36)
#define GITS_TYPER_CIDbits   BITFIELD(35, 32)
#define GITS_TYPER_HCC       BITFIELD(31, 24)
#define GITS_TYPER_PTA       BITFLAG(19)
#define GITS_TYPER_Devbits   BITFIELD(17, 13)
#define GITS_TYPER_ID_bits   BITFIELD(12, 8)
#define GITS_TYPER_ITTe_sz   BITFIELD(7, 4)

#define GITS_BASER_VALID     BITFLAG(63)
#define GITS_BASER_Ind       BITFLAG(62)
#define GITS_BASER_ICACHE    BITFIELD(61, 59)
#define GITS_BASER_OCACHE    BITFIELD(55, 53)
#define GITS_BASER_PA        BITFIELD(47, 12)
#define GITS_BASER_SHARE     BITFIELD(11, 10)
#define GITS_BASER_SIZE      BITFIELD(7, 0)

#define GITS_BASERn_TYPE     BITFIELD(58, 56)
#define GITS_BASERn_EntSz    BITFIELD(52, 48)
#define GITS_BASERn_PGSZ     BITFIELD(9, 8)

#define GITS_CWRRD_OFF       BITFIELD(19, 5)

int
gic_handle_irq(struct hart_state* hs);

#endif /* __LUNAIX_AA64_GIC_H */
