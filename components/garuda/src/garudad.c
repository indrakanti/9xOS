/*
 * 9xOS Garuda - garudad, the liveness monitor daemon.
 *
 * Listens on an AF_UNIX datagram socket for REGISTER / KICK / UNREGISTER
 * messages, checks every client's deadline, and feeds the hardware watchdog
 * only while every registered client is healthy. On any fault it stops
 * feeding the watchdog, so the hardware resets the board (the safe state).
 *
 * The kernel should be built with CONFIG_WATCHDOG_NOWAYOUT=y so that a
 * crash or kill of garudad also ends in a watchdog reset.
 */
#define _GNU_SOURCE
#include "garuda/garuda_proto.h"
#include "monitor.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#define EXIT_FAULT 2

struct config {
    const char *socket_path;
    const char *watchdog_path;   /* NULL = no hardware watchdog (development only) */
    uint32_t    feed_ms;
    int         exit_on_fault;
    int         foreground;
};

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static uint64_t now_ms(void)
{
    struct timespec ts;

    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static void on_fault(const gm_entry_t *e, uint64_t t, void *ctx)
{
    (void)ctx;
    syslog(LOG_CRIT, "FAULT client=%s pid=%" PRId32 " missed deadline (period=%" PRIu32
           " ms, late by %" PRIu64 " ms) - entering safe state",
           e->name, e->pid, e->period_ms, t - e->deadline_ms);
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s [-s socket] [-w watchdog_dev] [-f feed_ms] [-x] [-F]\n"
            "  -s PATH  control socket (default %s)\n"
            "  -w PATH  hardware watchdog device, e.g. /dev/watchdog0\n"
            "  -f MS    watchdog feed interval in ms (default 500)\n"
            "  -x       exit with status %d on the first fault (testing)\n"
            "  -F       log to stderr as well as syslog\n",
            prog, GARUDA_DEFAULT_SOCKET, EXIT_FAULT);
}

static int parse_args(int argc, char **argv, struct config *cfg)
{
    int opt;

    cfg->socket_path = GARUDA_DEFAULT_SOCKET;
    cfg->watchdog_path = NULL;
    cfg->feed_ms = 500u;
    cfg->exit_on_fault = 0;
    cfg->foreground = 0;

    while ((opt = getopt(argc, argv, "s:w:f:xFh")) != -1) {
        switch (opt) {
        case 's': cfg->socket_path = optarg; break;
        case 'w': cfg->watchdog_path = optarg; break;
        case 'f': {
            char *end = NULL;
            unsigned long v = strtoul(optarg, &end, 10);
            if (end == optarg || *end != '\0' || v < 10u || v > 60000u) {
                fprintf(stderr, "invalid feed interval: %s\n", optarg);
                return -1;
            }
            cfg->feed_ms = (uint32_t)v;
            break;
        }
        case 'x': cfg->exit_on_fault = 1; break;
        case 'F': cfg->foreground = 1; break;
        default:  usage(argv[0]); return -1;
        }
    }
    return 0;
}

static int open_socket(const char *path)
{
    struct sockaddr_un addr;
    int fd;
    int one = 1;
    mode_t old;

    if (strlen(path) >= sizeof(addr.sun_path)) {
        syslog(LOG_ERR, "socket path too long: %s", path);
        return -1;
    }
    fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "socket: %s", strerror(errno));
        return -1;
    }
    /* Kernel attaches the sender's real credentials to every datagram. */
    if (setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &one, sizeof(one)) != 0) {
        syslog(LOG_ERR, "SO_PASSCRED: %s", strerror(errno));
        (void)close(fd);
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, path, strlen(path) + 1u);

    (void)unlink(path);
    old = umask(0117);               /* socket mode 0660: owner + group only */
    if (bind(fd, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
        syslog(LOG_ERR, "bind %s: %s", path, strerror(errno));
        (void)umask(old);
        (void)close(fd);
        return -1;
    }
    (void)umask(old);
    return fd;
}

/* Receive one message; returns 1 if a valid-size message with credentials
 * was read, 0 if nothing (or junk) was read, -1 on hard error. */
static int receive(int fd, struct garuda_msg *m, int32_t *pid)
{
    union {
        char buf[CMSG_SPACE(sizeof(struct ucred))];
        struct cmsghdr align;
    } ctrl;
    struct iovec iov = { .iov_base = m, .iov_len = sizeof(*m) };
    struct msghdr mh;
    struct cmsghdr *c;
    ssize_t n;

    memset(&mh, 0, sizeof(mh));
    mh.msg_iov = &iov;
    mh.msg_iovlen = 1;
    mh.msg_control = ctrl.buf;
    mh.msg_controllen = sizeof(ctrl.buf);

    n = recvmsg(fd, &mh, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
    if (n < 0) {
        return (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) ? 0 : -1;
    }
    if (n != (ssize_t)sizeof(*m) || (mh.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) != 0) {
        syslog(LOG_WARNING, "dropped malformed datagram (%zd bytes)", n);
        return 0;
    }
    *pid = 0;
    for (c = CMSG_FIRSTHDR(&mh); c != NULL; c = CMSG_NXTHDR(&mh, c)) {
        if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_CREDENTIALS) {
            struct ucred cred;
            memcpy(&cred, CMSG_DATA(c), sizeof(cred));
            *pid = (int32_t)cred.pid;
        }
    }
    if (*pid <= 0) {
        syslog(LOG_WARNING, "dropped datagram without credentials");
        return 0;
    }
    return 1;
}

static int feed_watchdog(int wd)
{
    if (wd < 0) {
        return 0;
    }
    return (write(wd, "k", 1) == 1) ? 0 : -1;
}

int main(int argc, char **argv)
{
    struct config cfg;
    struct sigaction sa;
    gm_state_t state;
    int sock;
    int wd = -1;
    int faulted = 0;
    uint64_t next_feed;

    if (parse_args(argc, argv, &cfg) != 0) {
        return EXIT_FAILURE;
    }
    openlog("garudad", LOG_PID | (cfg.foreground ? LOG_PERROR : 0), LOG_DAEMON);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    (void)sigaction(SIGTERM, &sa, NULL);
    (void)sigaction(SIGINT, &sa, NULL);

    sock = open_socket(cfg.socket_path);
    if (sock < 0) {
        return EXIT_FAILURE;
    }
    if (cfg.watchdog_path != NULL) {
        wd = open(cfg.watchdog_path, O_WRONLY | O_CLOEXEC);
        if (wd < 0) {
            syslog(LOG_ERR, "open %s: %s", cfg.watchdog_path, strerror(errno));
            (void)close(sock);
            return EXIT_FAILURE;
        }
    } else {
        syslog(LOG_WARNING, "no hardware watchdog configured - development mode, faults are only logged");
    }

    gm_init(&state);
    syslog(LOG_INFO, "9xOS Garuda started (socket=%s watchdog=%s feed=%" PRIu32 " ms)",
           cfg.socket_path, cfg.watchdog_path ? cfg.watchdog_path : "none", cfg.feed_ms);

    next_feed = now_ms();
    while (!g_stop) {
        struct pollfd pfd = { .fd = sock, .events = POLLIN, .revents = 0 };
        uint64_t t = now_ms();
        uint64_t wake = gm_next_deadline(&state);
        int timeout;

        if (wake != UINT64_MAX) {
            wake += 1u;                      /* deadline is exceeded strictly after */
        }
        if (next_feed < wake) {
            wake = next_feed;
        }
        timeout = (wake <= t) ? 0 : (int)((wake - t) > 60000u ? 60000u : (wake - t));

        if (poll(&pfd, 1, timeout) < 0 && errno != EINTR) {
            syslog(LOG_ERR, "poll: %s", strerror(errno));
            break;
        }

        if ((pfd.revents & POLLIN) != 0) {
            struct garuda_msg m;
            int32_t pid;
            int rc;

            while ((rc = receive(sock, &m, &pid)) == 1) {
                gm_result_t r = gm_handle(&state, &m, pid, now_ms());
                if (r != GM_OK) {
                    syslog(LOG_WARNING, "rejected msg type=%u from pid=%" PRId32 ": %s",
                           (unsigned)m.type, pid, gm_result_str(r));
                } else if (m.type == GARUDA_MSG_REGISTER) {
                    syslog(LOG_INFO, "registered client=%.*s pid=%" PRId32 " period=%" PRIu32 " ms",
                           (int)GARUDA_NAME_MAX, m.name, pid, m.period_ms);
                } else if (m.type == GARUDA_MSG_UNREGISTER) {
                    syslog(LOG_INFO, "unregistered client=%.*s pid=%" PRId32,
                           (int)GARUDA_NAME_MAX, m.name, pid);
                }
            }
            if (rc < 0) {
                syslog(LOG_ERR, "recvmsg: %s", strerror(errno));
                break;
            }
        }

        t = now_ms();
        (void)gm_check(&state, t, on_fault, NULL);

        if (!gm_healthy(&state)) {
            if (!faulted) {
                faulted = 1;
                syslog(LOG_CRIT, "system unhealthy - watchdog feeding stopped");
                if (cfg.exit_on_fault) {
                    break;
                }
            }
        } else if (t >= next_feed) {
            if (feed_watchdog(wd) != 0) {
                syslog(LOG_ERR, "watchdog feed failed: %s", strerror(errno));
            }
            next_feed = t + cfg.feed_ms;
        }
    }

    /* No magic close: with NOWAYOUT the watchdog stays armed after exit. */
    if (wd >= 0) {
        (void)close(wd);
    }
    (void)close(sock);
    (void)unlink(cfg.socket_path);
    syslog(LOG_INFO, "stopped (%s)", faulted ? "faulted" : "clean");
    closelog();
    return faulted ? EXIT_FAULT : EXIT_SUCCESS;
}
