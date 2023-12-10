#include <lunaix/kcmd.h>
#include <lunaix/syslog.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

#include <klibc/string.h>

LOG_MODULE("kcmd");

struct substr {
    int pos;
    size_t len;
};

struct extractor {
    char* cmdline;
    int pos;

    struct substr key;
    struct substr val;
};

static DECLARE_HASHTABLE(options, 8);

static bool
extract_next_option(struct extractor* ctx) 
{
#define is_valid_char(c) ((unsigned int)(((c) & ~0x80) - 'A') < 'Z')
#define PARSE_KEY 0
#define PARSE_VAL 1
    char c;
    int i = ctx->pos, state = PARSE_KEY;
    struct substr* s = &ctx->key;
    s->len = 0;
    s->pos = i;

    if (!ctx->cmdline[i]) {
        return false;
    }

    while((c = ctx->cmdline[i++])) {
        if (c == ' ') {
            while ((c = ctx->cmdline[i++]) && c == ' ');
            break;
        }

        if (c == '=') {
            if (state == PARSE_KEY) {
                if (s->len == 0) {
                    WARN("unexpected character: '=' (pos:%d). skipping...\n", i + 1);
                    while ((c = ctx->cmdline[i++]) && c != ' ');
                    break;
                }
                state = PARSE_VAL;
                s = &ctx->val;
                s->len = 0;
                s->pos = i;
                continue;
            }
        }

        s->len++;
    }

    ctx->pos = i - 1;

    return true;
}

#define MAX_KEYSIZE 16

void 
kcmd_parse_cmdline(char* cmd_line) 
{
    struct extractor ctx = { .cmdline = cmd_line, .pos = 0 };
    
    while(extract_next_option(&ctx)) {
        if (!ctx.key.len) {
            continue;
        }
        
        size_t maxlen = sizeof(struct koption) - offsetof(struct koption, buf);

        if (ctx.key.len >= maxlen) {
            WARN("option key too big. skipping...\n");
            continue;
        }

        if (ctx.val.len >= 256) {
            WARN("option value too big. skipping...\n");
            continue;
        }

        struct koption* kopt = 
            (struct koption*) vzalloc(sizeof(*kopt));
        
        memcpy(kopt->buf, &cmd_line[ctx.key.pos], ctx.key.len);
        
        if (ctx.val.len) {
            kopt->value = &kopt->buf[ctx.key.len + 1];
            size_t max_val_len = maxlen - ctx.key.len;

            // value too big to fit inplace
            if (max_val_len <= ctx.val.len) {
                kopt->value = vzalloc(ctx.val.len + 1);
            }

            memcpy(kopt->value, &cmd_line[ctx.val.pos], ctx.val.len);
        }
        
        kopt->hashkey = HSTR(kopt->buf, ctx.key.len);
        hstr_rehash(&kopt->hashkey, HSTR_FULL_HASH);

        hashtable_hash_in(options, &kopt->node, kopt->hashkey.hash);
    }
}

bool 
kcmd_get_option(char* key, char** out_value) 
{
    struct hstr hkey = HSTR(key, strlen(key));
    hstr_rehash(&hkey, HSTR_FULL_HASH);

    struct koption *pos, *n;
    hashtable_hash_foreach(options, hkey.hash, pos, n, node) {
        if (HSTR_EQ(&pos->hashkey, &hkey)) {
            goto found;
        }
    }

    return false;

found:
    if (out_value) {
        *out_value = pos->value;
    }

    return true;
}