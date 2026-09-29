/* 9xOS Garuda - monitor core (see monitor.h for the rules). */
#include "monitor.h"

#include <string.h>

static bool name_valid(const char name[GARUDA_NAME_MAX])
{
    /* Must be non-empty and NUL-terminated inside the buffer. */
    return (name[0] != '\0') && (memchr(name, '\0', GARUDA_NAME_MAX) != NULL);
}

static gm_entry_t *find(gm_state_t *s, const char *name)
{
    uint32_t i;

    for (i = 0u; i < GM_MAX_CLIENTS; i++) {
        if (s->entry[i].used && strncmp(s->entry[i].name, name, GARUDA_NAME_MAX) == 0) {
            return &s->entry[i];
        }
    }
    return NULL;
}

static gm_entry_t *alloc_slot(gm_state_t *s)
{
    uint32_t i;

    for (i = 0u; i < GM_MAX_CLIENTS; i++) {
        if (!s->entry[i].used) {
            return &s->entry[i];
        }
    }
    return NULL;
}

void gm_init(gm_state_t *s)
{
    memset(s, 0, sizeof(*s));
}

static gm_result_t do_register(gm_state_t *s, const struct garuda_msg *m, int32_t pid, uint64_t now_ms)
{
    gm_entry_t *e;

    if (m->period_ms < GM_MIN_PERIOD_MS || m->period_ms > GM_MAX_PERIOD_MS) {
        return GM_ERR_INVALID;
    }
    e = find(s, m->name);
    if (e != NULL) {
        if (e->pid != pid) {
            return GM_ERR_PID;               /* GRD-REQ-03 */
        }
        if (e->faulted) {
            return GM_ERR_FAULTED;           /* GRD-REQ-02: must unregister first */
        }
    } else {
        e = alloc_slot(s);
        if (e == NULL) {
            return GM_ERR_FULL;
        }
    }
    memset(e, 0, sizeof(*e));
    e->used = true;
    e->pid = pid;
    e->period_ms = m->period_ms;
    e->deadline_ms = now_ms + m->period_ms;
    memcpy(e->name, m->name, GARUDA_NAME_MAX);
    e->name[GARUDA_NAME_MAX - 1u] = '\0';
    return GM_OK;
}

static gm_result_t do_kick(gm_state_t *s, const struct garuda_msg *m, int32_t pid, uint64_t now_ms)
{
    gm_entry_t *e = find(s, m->name);

    if (e == NULL) {
        return GM_ERR_UNKNOWN;
    }
    if (e->pid != pid) {
        return GM_ERR_PID;                   /* GRD-REQ-03 */
    }
    if (e->faulted) {
        return GM_ERR_FAULTED;               /* GRD-REQ-02 */
    }
    /* Wrap-safe "seq > last_seq" (GRD-REQ-04). */
    if ((int32_t)(m->seq - e->last_seq) <= 0) {
        return GM_ERR_SEQ;
    }
    /* A kick that arrives after the deadline does not rescue the client;
     * gm_check() is authoritative, so evaluate the deadline here as well. */
    if (now_ms > e->deadline_ms) {
        return GM_ERR_FAULTED;
    }
    e->last_seq = m->seq;
    e->deadline_ms = now_ms + e->period_ms;
    return GM_OK;
}

static gm_result_t do_unregister(gm_state_t *s, const struct garuda_msg *m, int32_t pid)
{
    gm_entry_t *e = find(s, m->name);

    if (e == NULL) {
        return GM_ERR_UNKNOWN;
    }
    if (e->pid != pid) {
        return GM_ERR_PID;
    }
    memset(e, 0, sizeof(*e));
    return GM_OK;
}

gm_result_t gm_handle(gm_state_t *s, const struct garuda_msg *m, int32_t pid, uint64_t now_ms)
{
    /* GRD-REQ-05: validate everything before touching state. */
    if (s == NULL || m == NULL || pid <= 0) {
        return GM_ERR_INVALID;
    }
    if (m->magic != GARUDA_PROTO_MAGIC || m->version != GARUDA_PROTO_VERSION) {
        return GM_ERR_INVALID;
    }
    if (!name_valid(m->name)) {
        return GM_ERR_INVALID;
    }
    switch (m->type) {
    case GARUDA_MSG_REGISTER:
        return do_register(s, m, pid, now_ms);
    case GARUDA_MSG_KICK:
        return do_kick(s, m, pid, now_ms);
    case GARUDA_MSG_UNREGISTER:
        return do_unregister(s, m, pid);
    default:
        return GM_ERR_INVALID;
    }
}

uint32_t gm_check(gm_state_t *s, uint64_t now_ms, gm_fault_cb cb, void *ctx)
{
    uint32_t i;
    uint32_t newly = 0u;

    for (i = 0u; i < GM_MAX_CLIENTS; i++) {
        gm_entry_t *e = &s->entry[i];
        if (e->used && !e->faulted && now_ms > e->deadline_ms) {
            e->faulted = true;               /* GRD-REQ-01, latched per GRD-REQ-02 */
            newly++;
            if (cb != NULL) {
                cb(e, now_ms, ctx);
            }
        }
    }
    return newly;
}

bool gm_healthy(const gm_state_t *s)
{
    uint32_t i;

    for (i = 0u; i < GM_MAX_CLIENTS; i++) {
        if (s->entry[i].used && s->entry[i].faulted) {
            return false;                    /* GRD-REQ-06 */
        }
    }
    return true;
}

uint32_t gm_active(const gm_state_t *s)
{
    uint32_t i;
    uint32_t n = 0u;

    for (i = 0u; i < GM_MAX_CLIENTS; i++) {
        if (s->entry[i].used) {
            n++;
        }
    }
    return n;
}

uint64_t gm_next_deadline(const gm_state_t *s)
{
    uint32_t i;
    uint64_t next = UINT64_MAX;

    for (i = 0u; i < GM_MAX_CLIENTS; i++) {
        const gm_entry_t *e = &s->entry[i];
        if (e->used && !e->faulted && e->deadline_ms < next) {
            next = e->deadline_ms;
        }
    }
    return next;
}

const char *gm_result_str(gm_result_t r)
{
    switch (r) {
    case GM_OK:          return "ok";
    case GM_ERR_INVALID: return "invalid message";
    case GM_ERR_FULL:    return "client table full";
    case GM_ERR_UNKNOWN: return "unknown client";
    case GM_ERR_PID:     return "pid mismatch";
    case GM_ERR_SEQ:     return "stale sequence";
    case GM_ERR_FAULTED: return "client faulted";
    default:             return "unknown result";
    }
}
