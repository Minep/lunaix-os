#ifndef __LUNAIX_USRSCOPE_H
#define __LUNAIX_USRSCOPE_H

#include <lunaix/types.h>
#include "usercaps.h"

struct ugroup_obj
{
    gid_t* list;
    unsigned int maxcap;
    unsigned int refs;
};

struct user_scope
{
    uid_t ruid;
    gid_t rgid;
    caps_t capabilities;

    struct ugroup_obj* grps;
};

/*
    Process ACL check result. 
    The encoding here is designed to be the mask for file ACL value
*/
enum acl_match
{
    ACL_MATCH_U  = 0b111000000, // (U)ser
    ACL_MATCH_G  = 0b000111000, // (G)roup
    ACL_NO_MATCH = 0b000000111, // (O)ther
};

#define user_groups(uscope)     ((uscope)->grps)

#define grp_list_end       ((gid_t)-1)

int 
uscope_setgroups(struct user_scope* proc_usr, 
                 const gid_t* grps, unsigned int len);

void 
uscope_copy(struct user_scope* from, struct user_scope* to);

bool
uscope_membership(struct user_scope* proc_usr, gid_t gid);

enum acl_match
check_current_acl(uid_t desired_u, gid_t desired_g);

static inline bool
uscope_with_capability(const struct user_scope* proc_usr, caps_t cap)
{
    return !!(proc_usr->capabilities & (1UL << cap));
}

#endif /* __LUNAIX_USRSCOPE_H */
