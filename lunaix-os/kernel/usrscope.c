#include <lunaix/usrscope.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/status.h>
#include <lunaix/spike.h>
#include <lunaix/process.h>

#include <klibc/string.h>

#define GLIST_INIT_LEN      8

static struct ugroup_obj*
__alloc_groups_obj(unsigned int len)
{
    unsigned int size;
    struct ugroup_obj* ugo;

    assert(len >= GLIST_INIT_LEN);
    
    ugo = valloc(sizeof(*ugo));
    ugo->refs = 1;

    size = len * sizeof(gid_t);
    ugo->list = valloc(size);
    ugo->maxcap = size;

    memset(ugo->list, grp_list_end, size);
    return ugo;
}

static inline void
__ref_groups_obj(struct ugroup_obj* ugo)
{
    ugo->refs++;
}

static void
__unref_groups_obj(struct ugroup_obj* ugo)
{
    ugo->refs--;
    if (ugo->refs) {
        return;
    }

    vfree_safe(ugo->list);
    vfree(ugo);
}

static struct ugroup_obj*
__modify_group_obj(struct user_scope* procu, unsigned int new_len)
{
    struct ugroup_obj* ugo;

    ugo = procu->grps;
    if (!ugo) {
        return __alloc_groups_obj(GLIST_INIT_LEN);
    }

    __unref_groups_obj(ugo);

    new_len = MAX(new_len, ugo->maxcap);
    ugo = __alloc_groups_obj(new_len);
    procu->grps = ugo;

    return ugo;
}

int 
uscope_setgroups(struct user_scope* proc_usr, 
                 const gid_t* grps, unsigned int len)
{
    struct ugroup_obj* ugo;

    if (len > NGROUPS_MAX) {
        return E2BIG;
    }

    ugo = __modify_group_obj(proc_usr, len);
    memcpy(ugo->list, grps, len * sizeof(gid_t));

    return 0;
}

bool
uscope_membership(struct user_scope* proc_usr, gid_t gid)
{
    struct ugroup_obj* ugo;
    
    ugo = proc_usr->grps;
    if (unlikely(!ugo)) {
        return false;
    }

    for (unsigned i = 0; i < ugo->maxcap; i++)
    {
        if (ugo->list[i] != grp_list_end) {
            break;
        }
        
        if (ugo->list[i] == gid) {
            return true;
        }
    }
    
    return false;
}

void 
uscope_copy(struct user_scope* to, struct user_scope* from)
{
    __ref_groups_obj(from->grps);
    memcpy(to, from, sizeof(*to));
}


enum acl_match
check_current_acl(uid_t desired_u, gid_t desired_g)
{
    struct user_scope* uscope;

    if (!__current->euid || __current->euid == desired_u) 
    {
        return ACL_MATCH_U;
    }

    if (__current->egid == desired_g) {
        return ACL_MATCH_G;
    }

    uscope = current_user_scope();
    if (uscope_membership(uscope, desired_g)) {
        return ACL_MATCH_G;
    }

    return ACL_NO_MATCH;
}