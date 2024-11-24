
#define __off_t_defined

#include "dut/devtree.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

extern char *strerror(int);

#define MAX_DTB_SZ (2 * 1024 * 1024)

extern void init_export___init_devtree();

static void
__dump_tree(struct dt_node *root, int ident)
{
    morph_t *p, *n;
    struct dt_node *node;
    char prefixes[16];

    if (ident >= 16) {
        return;
    }
    
    int i;
    for (i = 0; i < ident; i++)
    {
        prefixes[i] = '\t';
    }
    prefixes[i] = 0;
    
    
    char* compat;
    changeling_for_each(p, n, &root->mobj)
    {
        node = changeling_reveal(p, dt_morpher);
        compat = node->base.compat.str_val;
        compat = compat ?: "n/a";

        printf("%s /%s (%s)\n", prefixes, 
                              node->mobj.name.value, 
                              compat);
        
        __dump_tree(node, ident + 1);
    }
}

static void
dump_dtb()
{
    struct dt_context* ctx;

    ctx = dt_main_context();
    
    __dump_tree(ctx->root, 0);
}

int main(int argc, char** argv)
{
    assert(argc == 2);

    const char* dtb_file = argv[1];

    int fd = open(dtb_file, O_RDONLY);
    if (fd == -1) {
        printf("fail to open: %s\n", strerror(errno));
        return 1;
    }

    ptr_t dtb = (ptr_t)mmap(0, 
                            MAX_DTB_SZ, 
                            PROT_READ | PROT_WRITE | MAP_POPULATE, 
                            MAP_PRIVATE, fd, 0);
    
    if (dtb == (ptr_t)-1) {
        printf("fail to map dtb: %s\n", strerror(errno));
        return 1;
    }

    close(fd);
    init_export___init_devtree();

    bool b = dt_load(dtb);

    if (b) {
        dump_dtb();
    }
    
    munmap((void*)dtb, MAX_DTB_SZ);

    return !b;
}