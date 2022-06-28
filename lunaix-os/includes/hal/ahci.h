#ifndef __LUNAIX_AHCI_H
#define __LUNAIX_AHCI_H

/*
 * Macro naming rule:
 *      HBA_R[xxx]
 *          HBA Register [xxx]
 *          e.g. HBA_RPxCLB is Register PxCLB
 *
 * All registers offset are 0 based index of a DWORD array
 */

#define HBA_RCAP 0
#define HBA_RGHC 1
#define HBA_RIS 2
#define HBA_RPI 3
#define HBA_RVER 4

#define HBA_RPBASE (0x40)
#define HBA_RPxCLB 0
#define HBA_RPxFB 2
#define HBA_RPxIS 4
#define HBA_RPxIE 5
#define HBA_RPxCMD 6
#define HBA_RPxTFD 8
#define HBA_RPxSIG 9
#define HBA_RPxSSTS 10
#define HBA_RPxSCTL 11
#define HBA_RPxSERR 12
#define HBA_RPxSACT 13
#define HBA_RPxCI 14
#define HBA_RPxSNTF 15
#define HBA_RPxFBS 16

typedef unsigned int hba_reg_t;

struct ahci_hba
{
    volatile hba_reg_t* base;
    unsigned int ports_num;
    unsigned int port_map;
    unsigned int version;
    hba_reg_t* ports[32];
};

void
ahci_init();

#endif /* __LUNAIX_AHCI_H */
