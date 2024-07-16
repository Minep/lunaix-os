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
    const char* argv[] = { "/mnt/lunaix-os/usr/bin/init", 0 };
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
    cpu_disable_interrupt();

    if (!mount_bootmedium() || !exec_initd()) {
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

    cpu_enable_interrupt();
    while (1)
    {
        cleanup_detached_threads();
        sched_pass();
    }
}

void
init_platform()
{
    device_postboot_load();
    invoke_init_function(on_postboot);

    twifs_register_plugins();

    // FIXME Re-design needed!!
    // sdbg_init();
    
    assert(!spawn_process(NULL, (ptr_t)lunad_do_usr, true));

    exit_thread(NULL);
}