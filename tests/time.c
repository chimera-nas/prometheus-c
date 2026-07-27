// SPDX-FileCopyrightText: 2025 Ben Jarvis
//
// SPDX-License-Identifier: LGPL-2.1-only

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <time.h>
#include "prometheus-c.h"

#define NUM_SAMPLES 5

int
main(
    int    argc,
    char **argv)
{
    struct prometheus_metrics            *metrics;
    struct prometheus_histogram          *histogram;
    struct prometheus_histogram_series   *series;
    struct prometheus_histogram_instance *instance;
    struct prometheus_stopwatch           sw;
    char                                 *buffer;
    int                                   i;

    (void) argc;
    (void) argv;

    buffer = malloc(1024 * 1024);

    metrics = prometheus_metrics_create((char *[]) { "global" }, (char *[]) { "root" }, 1);

    /* 33 power-of-two tick buckets span ~ns to ~1s. */
    histogram = prometheus_metrics_create_histogram_time(metrics,
                                                         "test_op_nanoseconds",
                                                         "Test operation latency in nanoseconds", 33);

    series = prometheus_histogram_create_series(histogram,
                                                (const char *[]) { "op" }, (const char *[]) { "sleep" },
                                                1);

    instance = prometheus_histogram_series_create_instance(series);

    for (i = 0; i < NUM_SAMPLES; i++) {
        struct timespec req = { .tv_sec = 0, .tv_nsec = 1000000 }; /* 1 ms */

        prometheus_stopwatch_start(&sw);
        nanosleep(&req, NULL);
        prometheus_time_histogram_sample(instance, &sw);
    }

    /* The instance is a plain per-owner accumulator; check it directly. */
    if (instance->count != NUM_SAMPLES) {
        fprintf(stderr, "expected count %d, got %" PRIu64 "\n", NUM_SAMPLES, instance->count);
        return 1;
    }

    if (instance->sum == 0) {
        fprintf(stderr, "expected non-zero elapsed ticks\n");
        return 1;
    }

    prometheus_metrics_scrape(metrics, buffer, 1024 * 1024);
    printf("%s\n", buffer);

    prometheus_metrics_destroy(metrics);

    free(buffer);

    return 0;
} /* main */
