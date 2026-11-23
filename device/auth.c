#include "auth.h"

#include <stdio.h>
#include <string.h>

void auth_table_init(AuthTable *t)
{
    memset(t, 0, sizeof *t);
    pthread_mutex_init(&t->lock, NULL);
}

/* 16 byte /dev/urandom -> 32 ký tự hex thường (giống app_id của app). */
static int rand_token(char out[AUTH_TOKEN_LEN + 1])
{
    static const char hex[] = "0123456789abcdef";
    unsigned char bytes[AUTH_TOKEN_LEN / 2];
    FILE *f;
    size_t i;

    f = fopen("/dev/urandom", "rb");
    if (!f)
        return -1;
    if (fread(bytes, 1, sizeof bytes, f) != sizeof bytes) {
        fclose(f);
        return -1;
    }
    fclose(f);

    for (i = 0; i < sizeof bytes; i++) {
        out[i * 2] = hex[bytes[i] >> 4];
        out[i * 2 + 1] = hex[bytes[i] & 0x0f];
    }
    out[AUTH_TOKEN_LEN] = '\0';
    return 0;
}

/* Gọi với t->lock đang giữ. */
static int token_taken(AuthTable *t, const char *token)
{
    size_t i;
    for (i = 0; i < t->n; i++)
        if (strcmp(t->tokens[i].token, token) == 0)
            return 1;
    return 0;
}

int auth_issue(AuthTable *t, const char *app_id, char *out, size_t cap)
{
    char tok[AUTH_TOKEN_LEN + 1];
    int tries;

    if (!t || !app_id || !out || cap < AUTH_TOKEN_LEN + 1 ||
        strlen(app_id) >= AUTH_APP_ID_LEN)
        return -1;

    pthread_mutex_lock(&t->lock);
    for (tries = 0; tries < 16; tries++) {
        if (rand_token(tok) != 0)
            break;
        if (token_taken(t, tok))
            continue;
        if (t->n >= AUTH_MAX_TOKENS)
            break;
        snprintf(t->tokens[t->n].token, sizeof t->tokens[t->n].token,
                 "%s", tok);
        snprintf(t->tokens[t->n].app_id, sizeof t->tokens[t->n].app_id,
                 "%s", app_id);
        t->n++;
        memcpy(out, tok, sizeof tok);
        pthread_mutex_unlock(&t->lock);
        return 0;
    }
    pthread_mutex_unlock(&t->lock);
    return -1;
}

int auth_check(AuthTable *t, const char *app_id, const char *token)
{
    size_t i;
    int ok = 0;

    if (!t || !app_id || !token)
        return 0;

    pthread_mutex_lock(&t->lock);
    for (i = 0; i < t->n; i++) {
        if (strcmp(t->tokens[i].token, token) == 0 &&
            strcmp(t->tokens[i].app_id, app_id) == 0) {
            ok = 1;
            break;
        }
    }
    pthread_mutex_unlock(&t->lock);
    return ok;
}

int auth_revoke_app(AuthTable *t, const char *app_id)
{
    size_t i = 0;
    int removed = 0;

    if (!t || !app_id)
        return 0;

    pthread_mutex_lock(&t->lock);
    while (i < t->n) {
        if (strcmp(t->tokens[i].app_id, app_id) == 0) {
            t->tokens[i] = t->tokens[t->n - 1];
            t->n--;
            removed++;
        } else {
            i++;
        }
    }
    pthread_mutex_unlock(&t->lock);
    return removed;
}
