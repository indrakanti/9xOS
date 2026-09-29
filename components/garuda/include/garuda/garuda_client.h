/*
 * 9xOS Garuda - client API for applications supervised by garudad.
 *
 * Typical use:
 *
 *   garuda_client_t g;
 *   garuda_client_open(&g, NULL);           // NULL = $GARUDA_SOCKET or default
 *   garuda_register(&g, "perception", 100); // must kick at least every 100 ms
 *   for (;;) { do_cycle(); garuda_kick(&g); }
 *   garuda_unregister(&g);                  // clean shutdown only
 *   garuda_client_close(&g);
 *
 * All functions return 0 on success or a negative errno value.
 * Faults latch: once garudad has declared a client faulted, further kicks
 * do not clear it. Recovery requires unregister + register.
 */
#ifndef GARUDA_CLIENT_H
#define GARUDA_CLIENT_H

#include <stdint.h>
#include "garuda/garuda_proto.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct garuda_client {
    int      fd;
    uint32_t seq;
    char     name[GARUDA_NAME_MAX];
} garuda_client_t;

int  garuda_client_open(garuda_client_t *c, const char *socket_path);
int  garuda_register(garuda_client_t *c, const char *name, uint32_t period_ms);
int  garuda_kick(garuda_client_t *c);
int  garuda_unregister(garuda_client_t *c);
void garuda_client_close(garuda_client_t *c);

#ifdef __cplusplus
}
#endif

#endif /* GARUDA_CLIENT_H */
