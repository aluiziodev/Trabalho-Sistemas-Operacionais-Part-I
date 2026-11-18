#!/usr/bin/env bash
set -eu

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT_DIR"

ITERATIONS="${1:-200000}"

cat > /tmp/polling_bench.c <<'C'
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static long long now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void wait_yield(int n) {
    for (int i = 0; i < n; ++i) {
        sched_yield();
    }
}

static void wait_sleep_us(int n, int usec) {
    for (int i = 0; i < n; ++i) {
        usleep(usec);
    }
}

int main(int argc, char **argv) {
    int n = 200000;
    if (argc > 1) {
        n = atoi(argv[1]);
    }

    long long t0 = now_ns();
    wait_yield(n);
    long long t1 = now_ns();

    long long t2 = now_ns();
    wait_sleep_us(n / 4, 100);
    long long t3 = now_ns();

    printf("yield_only_ms=%.3f\n", (t1 - t0) / 1000000.0);
    printf("yield_plus_usleep100us_ms=%.3f\n", (t3 - t2) / 1000000.0);
    printf("iterations=%d\n", n);
    return 0;
}
C

gcc -Wall -Wextra -O2 -std=gnu11 /tmp/polling_bench.c -o /tmp/polling_bench
/tmp/polling_bench "$ITERATIONS"
