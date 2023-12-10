#include <lunaix/boot_generic.h>
#include <lunaix/exec.h>
#include <lunaix/foptions.h>
#include <lunaix/fs.h>
#include <lunaix/fs/probe_boot.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>
#include <lunaix/types.h>
#include <lunaix/owloysius.h>

#include <klibc/string.h>

LOG_MODULE("PROC0")

void
init_platform();

int
mount_bootmedium()
{
    struct v_dnode* dnode;
    int errno = 0;
    struct device* dev = probe_boot_medium();
    if (!dev) {
        ERROR("fail to acquire device. (%d)", errno);
        return 0;
    }

    if ((errno = vfs_mount("/mnt/lunaix-os", "iso9660", dev, 0))) {
        ERROR("fail to mount boot medium. (%d)", errno);
        return 0;
    }

    return 1;
}

int
exec_initd()
{
    int errno = 0;

    if ((errno = exec_kexecve("/mnt/lunaix-os/usr/bin/init", NULL, NULL))) {
        goto fail;
    }

    fail("should not reach");

fail:
    ERROR("fail to load initd. (%d)", errno);
    return 0;
}

/**
 * @brief LunaixOS的零号进程，该进程永远为可执行。
 *
 * 这主要是为了保证调度器在没有进程可调度时依然有事可做。
 *
 * 同时，该进程也负责fork出我们的init进程。
 *
 */
void
__proc0()
{
    /*
     * We must defer boot code/data cleaning to here, after we successfully
     * escape that area
     */
    boot_cleanup();

    init_platform();

    init_proc_user_space(__current);

    if (!mount_bootmedium() || !exec_initd()) {
        FATAL("failed to initd");
        // should not reach
    }
}

void
init_platform()
{
    device_postboot_load();
    invoke_init_function(call_on_postboot);

    twifs_register_plugins();

    // FIXME Re-design needed!!
    // sdbg_init();
}