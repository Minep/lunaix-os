#include <lunaix/device.h>
#include <lunaix/owloysius.h>
#include <lunaix/spike.h>
#include <lunaix/kcmd.h>
#include <lunaix/fs.h>
#include <lunaix/syslog.h>

#include <hal/term.h>

LOG_MODULE("console")

static void 
setup_default_tty()  
{
    char* console_dev;
    if(!kcmd_get_option("console", &console_dev)) {
        FATAL("I am expecting a console!");
        // should not reach
    }

    struct v_dnode* dn;
    int err;

    if ((err = vfs_walk(NULL, console_dev, &dn, NULL, 0))) {
        FATAL("unable to set console: %s, err=%d", console_dev, err);
        // should not reach
    }

    struct device* dev = resolve_device(dn->inode->data);
    if (!dev) {
        FATAL("not a device: %s", console_dev);
        // should not reach
    }

    assert(device_addalias(NULL, dev_meta(dev), "tty"));
    
    // TODO implement capability list
    // for now, we just assume the parameter always pointed to valid device
    sysconsole = dev;
}
lunaix_initfn(setup_default_tty, call_on_boot);