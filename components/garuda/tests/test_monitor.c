/* 9xOS Garuda - unit tests for the monitor core (one test per safety rule). */
#include "monitor.h"

#include <stdio.h>
#include <string.h>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond) do { \
        g_checks++; \
        if (!(cond)) { \
            g_failures++; \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

static struct garuda_msg msg(uint16_t type, const char *name, uint32_t period, uint32_t seq)
{
    struct garuda_msg m;

    memset(&m, 0, sizeof(m));
    m.magic = GARUDA_PROTO_MAGIC;
    m.version = (uint16_t)GARUDA_PROTO_VERSION;
    m.type = type;
    m.period_ms = period;
    m.seq = seq;
    {
        size_t len = strlen(name);
        if (len > GARUDA_NAME_MAX - 1u) {
            len = GARUDA_NAME_MAX - 1u;
        }
        memcpy(m.name, name, len);
    }
    return m;
}

static gm_result_t reg(gm_state_t *s, const char *n, uint32_t p, int32_t pid, uint64_t t)
{
    struct garuda_msg m = msg(GARUDA_MSG_REGISTER, n, p, 0u);
    return gm_handle(s, &m, pid, t);
}

static gm_result_t kick(gm_state_t *s, const char *n, uint32_t seq, int32_t pid, uint64_t t)
{
    struct garuda_msg m = msg(GARUDA_MSG_KICK, n, 0u, seq);
    return gm_handle(s, &m, pid, t);
}

static gm_result_t unreg(gm_state_t *s, const char *n, int32_t pid)
{
    struct garuda_msg m = msg(GARUDA_MSG_UNREGISTER, n, 0u, 0u);
    return gm_handle(s, &m, pid, 0u);
}

static int g_cb_calls = 0;
static void count_cb(const gm_entry_t *e, uint64_t t, void *ctx)
{
    (void)e; (void)t; (void)ctx;
    g_cb_calls++;
}

/* GRD-REQ-01: missed deadline -> fault; kicks in time keep it healthy. */
static void test_deadline(void)
{
    gm_state_t s;
    gm_init(&s);
    CHECK(reg(&s, "a", 100u, 10, 1000u) == GM_OK);
    CHECK(gm_check(&s, 1100u, NULL, NULL) == 0u);          /* exactly at deadline: ok */
    CHECK(kick(&s, "a", 1u, 10, 1090u) == GM_OK);
    CHECK(gm_check(&s, 1190u, NULL, NULL) == 0u);
    CHECK(gm_healthy(&s));
    g_cb_calls = 0;
    CHECK(gm_check(&s, 1191u, count_cb, NULL) == 1u);       /* 1 ms late -> fault */
    CHECK(g_cb_calls == 1);
    CHECK(!gm_healthy(&s));
    CHECK(gm_check(&s, 5000u, count_cb, NULL) == 0u);       /* reported once */
    CHECK(g_cb_calls == 1);
}

/* GRD-REQ-02: faults latch; kicks and re-register cannot clear them. */
static void test_latch(void)
{
    gm_state_t s;
    gm_init(&s);
    CHECK(reg(&s, "a", 50u, 10, 0u) == GM_OK);
    (void)gm_check(&s, 100u, NULL, NULL);
    CHECK(!gm_healthy(&s));
    CHECK(kick(&s, "a", 1u, 10, 101u) == GM_ERR_FAULTED);
    CHECK(reg(&s, "a", 50u, 10, 102u) == GM_ERR_FAULTED);
    CHECK(!gm_healthy(&s));
    CHECK(unreg(&s, "a", 10) == GM_OK);                     /* explicit recovery */
    CHECK(gm_healthy(&s));
    CHECK(reg(&s, "a", 50u, 10, 200u) == GM_OK);
}

/* A kick that arrives after the deadline must not rescue the client. */
static void test_late_kick(void)
{
    gm_state_t s;
    gm_init(&s);
    CHECK(reg(&s, "a", 100u, 10, 0u) == GM_OK);
    CHECK(kick(&s, "a", 1u, 10, 150u) == GM_ERR_FAULTED);
    CHECK(gm_check(&s, 150u, NULL, NULL) == 1u);
    CHECK(!gm_healthy(&s));
}

/* GRD-REQ-03: only the owning PID may kick / unregister / re-register. */
static void test_pid(void)
{
    gm_state_t s;
    gm_init(&s);
    CHECK(reg(&s, "a", 100u, 10, 0u) == GM_OK);
    CHECK(kick(&s, "a", 1u, 11, 10u) == GM_ERR_PID);
    CHECK(unreg(&s, "a", 11) == GM_ERR_PID);
    CHECK(reg(&s, "a", 100u, 11, 10u) == GM_ERR_PID);
    CHECK(kick(&s, "a", 1u, 10, 10u) == GM_OK);
}

/* GRD-REQ-04: sequence numbers strictly increase, wrap-safe. */
static void test_seq(void)
{
    gm_state_t s;
    gm_init(&s);
    CHECK(reg(&s, "a", 100u, 10, 0u) == GM_OK);
    CHECK(kick(&s, "a", 0u, 10, 1u) == GM_ERR_SEQ);
    CHECK(kick(&s, "a", 5u, 10, 2u) == GM_OK);
    CHECK(kick(&s, "a", 5u, 10, 3u) == GM_ERR_SEQ);        /* replay */
    CHECK(kick(&s, "a", 4u, 10, 4u) == GM_ERR_SEQ);        /* stale */
    s.entry[0].last_seq = 0xFFFFFFFEu;
    CHECK(kick(&s, "a", 0xFFFFFFFFu, 10, 5u) == GM_OK);
    CHECK(kick(&s, "a", 1u, 10, 6u) == GM_OK);             /* wrap */
}

/* GRD-REQ-05: malformed input is rejected and leaves state unchanged. */
static void test_invalid(void)
{
    gm_state_t s, before;
    struct garuda_msg m;
    gm_init(&s);
    CHECK(reg(&s, "a", 100u, 10, 0u) == GM_OK);
    memcpy(&before, &s, sizeof(s));

    m = msg(GARUDA_MSG_KICK, "a", 0u, 1u); m.magic = 0u;
    CHECK(gm_handle(&s, &m, 10, 1u) == GM_ERR_INVALID);
    m = msg(GARUDA_MSG_KICK, "a", 0u, 1u); m.version = 99u;
    CHECK(gm_handle(&s, &m, 10, 1u) == GM_ERR_INVALID);
    m = msg(99u, "a", 0u, 1u);
    CHECK(gm_handle(&s, &m, 10, 1u) == GM_ERR_INVALID);
    m = msg(GARUDA_MSG_KICK, "a", 0u, 1u); memset(m.name, 'x', sizeof(m.name)); /* no NUL */
    CHECK(gm_handle(&s, &m, 10, 1u) == GM_ERR_INVALID);
    m = msg(GARUDA_MSG_KICK, "", 0u, 1u);
    CHECK(gm_handle(&s, &m, 10, 1u) == GM_ERR_INVALID);
    m = msg(GARUDA_MSG_KICK, "a", 0u, 1u);
    CHECK(gm_handle(&s, &m, 0, 1u) == GM_ERR_INVALID);     /* no credentials */
    CHECK(gm_handle(&s, NULL, 10, 1u) == GM_ERR_INVALID);
    CHECK(reg(&s, "b", GM_MIN_PERIOD_MS - 1u, 10, 0u) == GM_ERR_INVALID);
    CHECK(reg(&s, "b", GM_MAX_PERIOD_MS + 1u, 10, 0u) == GM_ERR_INVALID);
    CHECK(kick(&s, "nobody", 1u, 10, 1u) == GM_ERR_UNKNOWN);

    CHECK(memcmp(&before, &s, sizeof(s)) == 0);
}

/* GRD-REQ-06 and capacity: table limits and health aggregation. */
static void test_capacity_and_health(void)
{
    gm_state_t s;
    char name[GARUDA_NAME_MAX];
    uint32_t i;
    gm_init(&s);
    CHECK(gm_healthy(&s));                                   /* empty = healthy */
    CHECK(gm_next_deadline(&s) == UINT64_MAX);
    for (i = 0u; i < GM_MAX_CLIENTS; i++) {
        (void)snprintf(name, sizeof(name), "c%u", (unsigned)i);
        CHECK(reg(&s, name, 100u + i, (int32_t)(100 + i), 0u) == GM_OK);
    }
    CHECK(gm_active(&s) == GM_MAX_CLIENTS);
    CHECK(reg(&s, "overflow", 100u, 999, 0u) == GM_ERR_FULL);
    CHECK(gm_next_deadline(&s) == 100u);
    CHECK(gm_check(&s, 101u, NULL, NULL) == 1u);             /* only c0 */
    CHECK(!gm_healthy(&s));
    CHECK(gm_next_deadline(&s) == 101u);                     /* faulted c0 excluded */
}

int main(void)
{
    test_deadline();
    test_latch();
    test_late_kick();
    test_pid();
    test_seq();
    test_invalid();
    test_capacity_and_health();

    printf("garuda monitor tests: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
