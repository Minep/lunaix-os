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
#include <lunaix/sched.h>
#include <lunaix/kpreempt.h>
#include <lunaix/kcmd.h>

#include <klibc/string.h>

LOG_MODULE("PROC0")

void
init_platform();

int
mount_bootmedium()
{
    int errno = 0;
    char* rootfs;
    struct v_dnode* dn;
    struct device* dev;

    if (!kcmd_get_option("rootfs", &rootfs)) {
        WARN("no prefered rootfs detected, try disks...");

        dev = probe_boot_medium();
        if (dev) {
            goto proceed;
        }

        ERROR("mount root: ran out options, can't proceed.");
        return 0;
    }

    if ((errno = vfs_walk(NULL, rootfs, &dn, NULL, 0))) {
        ERROR("%s: no such file (%d)", rootfs, errno);
        return 0;
    }
    
    dev = resolve_device(dn->inode->data);
    if (!dev) {
        ERROR("%s: not a device", rootfs);
        return 0;
    }

proceed:
    // unmount the /dev to put old root fs in clear
    must_success(vfs_unmount("/dev"));

    // re-mount the root fs with our device.
    if ((errno = vfs_mount_root(NULL, dev))) {
        ERROR("mount root failed: %s (%d)", rootfs, errno);
        return 0;
    }

    return 1;
}

int
exec_initd()
{
    int errno = 0;
    const char* argv[] = { "/init", 0 };
    const char* envp[] = { 0 };

    if ((errno = exec_kexecve(argv[0], argv, envp))) {
        goto fail;
    }

    fail("should not reach");

fail:
    ERROR("fail to load initd. (%d)", errno);
    return 0;
}

static void
lunad_do_usr() {
    // No, these are not preemptive
    no_preemption();

    if (!exec_initd()) {
        fail("failed to initd");
    }
}

/**
 * @brief LunaixOS的内核进程，该进程永远为可执行。
 *
 * 这主要是为了保证调度器在没有进程可调度时依然有事可做。
 *
 * 同时，该进程也负责fork出我们的init进程。
 *
 */
void _preemptible
lunad_main()
{
    spawn_kthread((ptr_t)init_platform);

    /*
        NOTE Kernel preemption after this point.

        More specifically, it is not a real kernel preemption (as in preemption
        happened at any point of kernel, except those marked explicitly).
        In Lunaix, things are designed in an non-preemptive fashion, we implement 
        kernel preemption the other way around: only selected kernel functions which,
        of course, with great care of preemptive assumption, will goes into kernel 
        thread (which is preemptive!)
    */

    set_preemption();
    while (1)
    {
        cleanup_detached_threads();
        yield_current();
    }
}

void
init_platform()
{    
    device_postboot_load();
    invoke_init_function(on_postboot);

    twifs_register_plugins();

    if (!mount_bootmedium()) {
        ERROR("failed to boot");
        goto exit;
    }

    // FIXME Re-design needed!!
    // sdbg_init();
    
    assert(!spawn_process(NULL, (ptr_t)lunad_do_usr, true));

exit:
    exit_thread(NULL);
}