#include <klibc/string.h>
#include <lunaix/ds/hstr.h>

void
hstrcpy(struct hstr* dest, struct hstr* src)
{
    strcpy(dest->value, src->value);
    dest->hash = src->hash;
    dest->len = src->len;
}