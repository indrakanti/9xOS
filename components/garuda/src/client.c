/* 9xOS Garuda - client library. */
#define _GNU_SOURCE
#include "garuda/garuda_client.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int send_msg(const garuda_client_t *c, uint16_t type, uint32_t period_ms, uint32_t seq)
{
    struct garuda_msg m;
    ssize_t n;

    memset(&m, 0, sizeof(m));
    m.magic = GARUDA_PROTO_MAGIC;
    m.version = (uint16_t)GARUDA_PROTO_VERSION;
    m.type = type;
    m.period_ms = period_ms;
    m.seq = seq;
    memcpy(m.name, c->name, sizeof(m.name));

    do {
        n = send(c->fd, &m, sizeof(m), MSG_NOSIGNAL);
    } while (n < 0 && errno == EINTR);

    if (n < 0) {
        return -errno;
    }
    return (n == (ssize_t)sizeof(m)) ? 0 : -EIO;
}

int garuda_client_open(garuda_client_t *c, const char *socket_path)
{
    struct sockaddr_un addr;
    const char *path = socket_path;

    if (c == NULL) {
        return -EINVAL;
    }
    memset(c, 0, sizeof(*c));
    c->fd = -1;

    if (path == NULL) {
        path = getenv(GARUDA_SOCKET_ENV);
    }
    if (path == NULL || path[0] == '\0') {
        path = GARUDA_DEFAULT_SOCKET;
    }
    if (strlen(path) >= sizeof(addr.sun_path)) {
        return -ENAMETOOLONG;
    }

    c->fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (c->fd < 0) {
        return -errno;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, path, strlen(path) + 1u);

    if (connect(c->fd, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
        int err = -errno;
        (void)close(c->fd);
        c->fd = -1;
        return err;
    }
    return 0;
}

int garuda_register(garuda_client_t *c, const char *name, uint32_t period_ms)
{
    size_t len;

    if (c == NULL || c->fd < 0 || name == NULL) {
        return -EINVAL;
    }
    len = strlen(name);
    if (len == 0u || len >= GARUDA_NAME_MAX) {
        return -EINVAL;
    }
    memset(c->name, 0, sizeof(c->name));
    memcpy(c->name, name, len);
    c->seq = 0u;
    return send_msg(c, (uint16_t)GARUDA_MSG_REGISTER, period_ms, 0u);
}

int garuda_kick(garuda_client_t *c)
{
    if (c == NULL || c->fd < 0 || c->name[0] == '\0') {
        return -EINVAL;
    }
    c->seq++;
    return send_msg(c, (uint16_t)GARUDA_MSG_KICK, 0u, c->seq);
}

int garuda_unregister(garuda_client_t *c)
{
    int rc;

    if (c == NULL || c->fd < 0 || c->name[0] == '\0') {
        return -EINVAL;
    }
    rc = send_msg(c, (uint16_t)GARUDA_MSG_UNREGISTER, 0u, 0u);
    if (rc == 0) {
        memset(c->name, 0, sizeof(c->name));
    }
    return rc;
}

void garuda_client_close(garuda_client_t *c)
{
    if (c != NULL && c->fd >= 0) {
        (void)close(c->fd);
        c->fd = -1;
    }
}
