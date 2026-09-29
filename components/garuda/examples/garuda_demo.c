/*
 * 9xOS Garuda - demo client.
 *
 * Registers, kicks every period/2 for COUNT cycles, then either unregisters
 * cleanly (-u) or simulates a hang by going silent (default).
 */
#define _GNU_SOURCE
#include "garuda/garuda_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void sleep_ms(unsigned ms)
{
    struct timespec ts = { .tv_sec = (time_t)(ms / 1000u), .tv_nsec = (long)(ms % 1000u) * 1000000L };
    while (nanosleep(&ts, &ts) != 0) {
    }
}

int main(int argc, char **argv)
{
    const char *name = "demo";
    const char *sock = NULL;
    unsigned period = 100u;
    unsigned count = 10u;
    unsigned hang_ms = 2000u;
    int clean = 0;
    int opt;
    unsigned i;
    int rc;
    garuda_client_t g;

    while ((opt = getopt(argc, argv, "n:p:c:s:H:u")) != -1) {
        switch (opt) {
        case 'n': name = optarg; break;
        case 'p': period = (unsigned)strtoul(optarg, NULL, 10); break;
        case 'c': count = (unsigned)strtoul(optarg, NULL, 10); break;
        case 's': sock = optarg; break;
        case 'H': hang_ms = (unsigned)strtoul(optarg, NULL, 10); break;
        case 'u': clean = 1; break;
        default:
            fprintf(stderr, "usage: %s [-n name] [-p period_ms] [-c kicks] [-s socket] [-H hang_ms] [-u]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    rc = garuda_client_open(&g, sock);
    if (rc != 0) {
        fprintf(stderr, "open: %s\n", strerror(-rc));
        return EXIT_FAILURE;
    }
    rc = garuda_register(&g, name, period);
    if (rc != 0) {
        fprintf(stderr, "register: %s\n", strerror(-rc));
        return EXIT_FAILURE;
    }
    for (i = 0u; i < count; i++) {
        sleep_ms(period / 2u);
        rc = garuda_kick(&g);
        if (rc != 0) {
            fprintf(stderr, "kick: %s\n", strerror(-rc));
            return EXIT_FAILURE;
        }
    }
    if (clean) {
        rc = garuda_unregister(&g);
        if (rc != 0) {
            fprintf(stderr, "unregister: %s\n", strerror(-rc));
            return EXIT_FAILURE;
        }
    } else {
        sleep_ms(hang_ms);                    /* simulated hang: no more kicks */
    }
    garuda_client_close(&g);
    return EXIT_SUCCESS;
}
