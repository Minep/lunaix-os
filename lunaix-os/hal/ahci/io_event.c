#include <hal/ahci/ahci.h>
#include <hal/ahci/sata.h>
#include <lunaix/generic/isrm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/syslog.h>

LOG_MODULE("io_evt")

void
ahci_hba_isr(const struct hart_state* hstate)
{
    struct ahci_hba* hba;
    struct ahci_driver *pos, *n;
    struct llist_header* ahcis = (struct llist_header*)isrm_get_payload(hstate);

    llist_for_each(pos, n, ahcis, ahci_drvs)
    {
        if (pos->id == hart_vector_stamp(hstate)) {
            hba = &pos->hba;
            goto proceed;
        }
    }

    return;

proceed:
    // ignore spurious interrupt
    if (!hba->base[HBA_RIS])
        return;

    u32_t port_num = msbiti - clz(hba->base[HBA_RIS]);
    struct hba_port* port = hba->ports[port_num];
    struct hba_cmd_context* cmdctx = &port->cmdctx;
    u32_t processed = port->regs[HBA_RPxCI] ^ cmdctx->tracked_ci;

    sata_read_error(port);

    // FIXME When error occurs, CI will not change. Need error recovery!
    if (!processed) {
        if (port->regs[HBA_RPxIS] & HBA_FATAL) {
            // TODO perform error recovery
            // This should include:
            //      1. Discard all issued (but pending) requests (signaled as
            //      error)
            //      2. Restart port
            // Complete steps refer to AHCI spec 6.2.2.1
        }
        goto done;
    }

    u32_t slot = msbiti - clz(processed);
    struct hba_cmd_state* cmdstate = cmdctx->issued[slot];

    if (!cmdstate) {
        goto done;
    }

    struct blkio_req* ioreq = (struct blkio_req*)cmdstate->state_ctx;

    if ((port->device->last_result.status & HBA_PxTFD_ERR)) {
        ioreq->errcode = port->regs[HBA_RPxTFD] & 0xffff;
        ioreq->flags |= BLKIO_ERROR;
        hba_clear_reg(port->regs[HBA_RPxSERR]);
    }

    blkio_schedule(ioreq->io_ctx);
    blkio_complete(ioreq);
    vfree_dma(cmdstate->cmd_table);

done:
    hba_clear_reg(port->regs[HBA_RPxIS]);
    hba->base[HBA_RIS] &= ~(1 << (31 - port_num));
}

void
__ahci_blkio_handler(struct blkio_req* req)
{
    struct hba_device* hbadev = (struct hba_device*)req->io_ctx->driver;

    hbadev->ops.submit(hbadev, req);
}