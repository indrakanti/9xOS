/*
 * 9xOS Garuda - liveness monitor wire protocol.
 *
 * One fixed-size datagram per message over an AF_UNIX SOCK_DGRAM socket.
 * The sender's PID is taken from kernel-supplied SCM_CREDENTIALS, never
 * from the payload, so a process cannot kick on behalf of another one.
 */
#ifndef GARUDA_PROTO_H
#define GARUDA_PROTO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GARUDA_PROTO_MAGIC   0x39584F53u /* "9XOS" */
#define GARUDA_PROTO_VERSION 1u
#define GARUDA_NAME_MAX      32u         /* including terminating NUL */

#define GARUDA_DEFAULT_SOCKET "/run/garuda.sock"
#define GARUDA_SOCKET_ENV     "GARUDA_SOCKET"

enum garuda_msg_type {
    GARUDA_MSG_REGISTER   = 1,
    GARUDA_MSG_KICK       = 2,
    GARUDA_MSG_UNREGISTER = 3
};

struct garuda_msg {
    uint32_t magic;      /* GARUDA_PROTO_MAGIC */
    uint16_t version;    /* GARUDA_PROTO_VERSION */
    uint16_t type;       /* enum garuda_msg_type */
    uint32_t period_ms;  /* REGISTER only: max time between kicks */
    uint32_t seq;        /* KICK only: must strictly increase */
    char     name[GARUDA_NAME_MAX];
};

#ifdef __cplusplus
}
#endif

#endif /* GARUDA_PROTO_H */
