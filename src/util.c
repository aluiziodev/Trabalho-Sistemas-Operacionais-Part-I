#include <time.h>

#include "util.h"

static struct timespec inicio_mono = {0, 0};

void clock_start(void) {
    clock_gettime(CLOCK_MONOTONIC, &inicio_mono);
}

long now_ms(void) {
    struct timespec agora;
    clock_gettime(CLOCK_MONOTONIC, &agora);

    if (inicio_mono.tv_sec == 0 && inicio_mono.tv_nsec == 0) {
        inicio_mono = agora;
    }

    long tempo_decorrido_ms = (agora.tv_sec - inicio_mono.tv_sec) * 1000L;
    tempo_decorrido_ms += (agora.tv_nsec - inicio_mono.tv_nsec) / 1000000L;
    return tempo_decorrido_ms;
}
