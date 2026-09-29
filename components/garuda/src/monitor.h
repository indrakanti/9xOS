/*
 * 9xOS Garuda - monitor core.
 *
 * Pure decision logic with no I/O and no dynamic memory: time is passed in
 * by the caller, so every rule here is unit-testable and deterministic.
 *
 * Safety rules implemented (see docs/safety/bhargav-0.1-safety-scope.md):
 *   GRD-REQ-01  A registered client that does not kick within period_ms is faulted.
 *   GRD-REQ-02  Faults latch; kicks do not clear them.
 *   GRD-REQ-03  Only the PID that registered a name may kick or unregister it.
 *   GRD-REQ-04  Kick sequence numbers must strictly increase (stale/replayed kicks rejected).
 *   GRD-REQ-05  Malformed messages are rejected and never change state.
 *   GRD-REQ-06  The system is healthy only if no client is faulted.
 */
#ifndef GARUDA_MONITOR_H
#define GARUDA_MONITOR_H

#include <stdbool.h>
#include <stdint.h>
#include "garuda/garuda_proto.h"

#define GM_MAX_CLIENTS     32u
#define GM_MIN_PERIOD_MS   10u
#define GM_MAX_PERIOD_MS   60000u

typedef enum {
    GM_OK = 0,
    GM_ERR_INVALID,   /* malformed message or bad parameters */
    GM_ERR_FULL,      /* client table full */
    GM_ERR_UNKNOWN,   /* name not registered */
    GM_ERR_PID,       /* name owned by another PID */
    GM_ERR_SEQ,       /* stale or replayed sequence number */
    GM_ERR_FAULTED    /* kick received for a latched-faulted client */
} gm_result_t;

typedef struct {
    bool     used;
    bool     faulted;
    int32_t  pid;
    uint32_t period_ms;
    uint32_t last_seq;
    uint64_t deadline_ms;
    char     name[GARUDA_NAME_MAX];
} gm_entry_t;

typedef struct {
    gm_entry_t entry[GM_MAX_CLIENTS];
} gm_state_t;

typedef void (*gm_fault_cb)(const gm_entry_t *e, uint64_t now_ms, void *ctx);

void        gm_init(gm_state_t *s);
gm_result_t gm_handle(gm_state_t *s, const struct garuda_msg *m, int32_t pid, uint64_t now_ms);
uint32_t    gm_check(gm_state_t *s, uint64_t now_ms, gm_fault_cb cb, void *ctx);
bool        gm_healthy(const gm_state_t *s);
uint32_t    gm_active(const gm_state_t *s);
uint64_t    gm_next_deadline(const gm_state_t *s);  /* UINT64_MAX if none */
const char *gm_result_str(gm_result_t r);

#endif /* GARUDA_MONITOR_H */
