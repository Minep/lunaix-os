#include <hal/ahci/hba.h>
#include <hal/ahci/sata.h>
#include <lunaix/isrm.h>
#include <lunaix/mm/valloc.h>

extern struct ahci_hba hba;

void
__ahci_hba_isr(const isr_param* param)
{
    // ignore spurious interrupt
    if (!hba.ports[HBA_RIS])
        return;

    u32_t port_num = 31 - __builtin_clz(hba.base[HBA_RIS]);
    struct hba_port* port = hba.ports[port_num];
    struct hba_cmd_context* cmdctx = &port->cmdctx;
    u32_t ci_filtered = port->regs[HBA_RPxCI] ^ cmdctx->tracked_ci;

    if (!ci_filtered) {
        goto done;
    }

    u32_t slot = 31 - __builtin_clz(ci_filtered);
    struct hba_cmd_state* cmdstate = cmdctx->issued[slot];

    if (!cmdstate) {
        goto done;
    }

    struct blkio_req* ioreq = (struct blkio_req*)cmdstate->state_ctx;
    sata_read_error(port);
    if ((port->device->last_result.status & HBA_PxTFD_ERR)) {
        ioreq->errcode = port->regs[HBA_RPxTFD] & 0xffff;
        ioreq->flags |= BLKIO_ERROR;
    }

    blkio_complete(ioreq);
    vfree(cmdstate->cmd_table);

done:
    hba_clear_reg(port->regs[HBA_RPxIS]);
}

void
__ahci_blkio_handler(struct blkio_req* req)
{
    struct hba_device* hbadev = (struct hba_device*)req->io_ctx->driver;

    hbadev->ops.submit(hbadev, req);
}