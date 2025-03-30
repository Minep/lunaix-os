#ifndef __LUNAIX_FS_ACL_H
#define __LUNAIX_FS_ACL_H

#include <lunaix/usrscope.h>
#include "compiler.h"

#define FSACL_READ     4
#define FSACL_WRITE    2
#define FSACL_EXEC     1

#define FSACL_RWXMASK  0777
#define FSACL_U(x)    (((x) & 0b111) << 6)
#define FSACL_G(x)    (((x) & 0b111) << 3)
#define FSACL_O(x)    ((x) & 0b111)

#define FSACL_uR      FSACL_U(FSACL_READ)
#define FSACL_uW      FSACL_U(FSACL_WRITE)
#define FSACL_uX      FSACL_U(FSACL_EXEC)

#define FSACL_gR      FSACL_G(FSACL_READ)
#define FSACL_gW      FSACL_G(FSACL_WRITE)
#define FSACL_gX      FSACL_G(FSACL_EXEC)

#define FSACL_oR      FSACL_O(FSACL_READ)
#define FSACL_oW      FSACL_O(FSACL_WRITE)
#define FSACL_oX      FSACL_O(FSACL_EXEC)

#define FSACL_suid    04000
#define FSACL_sgid    02000
#define FSACL_svtx    01000

// permitted read (any usr or group matched)
#define FSACL_RD      (FSACL_uRD | FSACL_gRD)
// permitted write (any usr or group matched)
#define FSACL_WR      (FSACL_uWR | FSACL_gWR)
// permitted execute (any usr or group matched)
#define FSACL_X       (FSACL_uX | FSACL_gX)

#define FSACL_u_      0
#define FSACL_g_      0
#define FSACL_o_      0

// any read
#define FSACL_aR      (FSACL_uR | FSACL_gR | FSACL_oR)
// any write
#define FSACL_aW      (FSACL_uW | FSACL_gW | FSACL_oW)
// any execute
#define FSACL_aX      (FSACL_uX | FSACL_gX | FSACL_oX)

#define __fsacl_sel(scope, type)    (FSACL_##scope##type)
#define FSACL_u(r, w, x)            \
        (v__(__fsacl_sel(u, r)) | v__(__fsacl_sel(u, w)) | v__(__fsacl_sel(u, x)))

#define FSACL_g(r, w, x)            \
        (v__(__fsacl_sel(g, r)) | v__(__fsacl_sel(g, w)) | v__(__fsacl_sel(g, x)))

#define FSACL_o(r, w, x)            \
        (v__(__fsacl_sel(o, r)) | v__(__fsacl_sel(o, w)) | v__(__fsacl_sel(o, x)))

#define fsacl_test(acl, type)   ((acl) & (FSACL_##type))

static inline bool must_inline
fsacl_allow_ops(unsigned int ops, unsigned int acl, uid_t uid, gid_t gid)
{
    return !!(acl & ops & check_current_acl(uid, gid));
}

#endif /* __LUNAIX_FS_ACL_H */
