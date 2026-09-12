// SPDX-FileCopyrightText: 2025 Ben Jarvis
//
// SPDX-License-Identifier: LGPL-2.1-only

#pragma once

#include <stdint.h>
#include "stopwatch.h"
#ifdef _WIN32
#include <intrin.h>
#ifdef PROMETHEUS_BUILD
#define PROMETHEUS_API __declspec(dllexport)
#else // ifdef PROMETHEUS_BUILD
#define PROMETHEUS_API __declspec(dllimport)
#endif // ifdef PROMETHEUS_BUILD
#else // ifdef _WIN32
#define PROMETHEUS_API __attribute__((visibility("default")))
#endif // ifdef _WIN32

static inline uint64_t
prometheus_log2(uint64_t value)
{
    if (!value) {
        return 0;
    }
#ifdef _WIN32
    unsigned long index;
    _BitScanReverse64(&index, value);
    return index;
#else // ifdef _WIN32
    return (uint64_t) (63 - __builtin_clzll(value));
#endif // ifdef _WIN32
} // prometheus_log2

struct prometheus_metrics;

struct prometheus_counter;
struct prometheus_counter_series;

struct prometheus_counter_instance {
    uint64_t value;
};

struct prometheus_gauge;
struct prometheus_gauge_series;

struct prometheus_gauge_instance {
    int64_t value;
};

struct prometheus_histogram;
struct prometheus_histogram_series;

enum prometheus_histogram_type {
    PROMETHEUS_HISTOGRAM_EXPONENTIAL,
    PROMETHEUS_HISTOGRAM_LINEAR,
    /*
     * Time histogram: buckets are accumulated in raw stopwatch ticks (TSC
     * cycles where available) using the same power-of-two scheme as
     * EXPONENTIAL. Bucket boundaries and the sum are converted to
     * nanoseconds only at scrape time.
     */
    PROMETHEUS_HISTOGRAM_TIME,
};

struct prometheus_histogram_instance {
    uint64_t  sum;
    uint64_t  count;
    uint64_t  start;
    uint64_t  increment;
    uint64_t  num_buckets;
    uint64_t *buckets;
    enum prometheus_histogram_type type;
};

PROMETHEUS_API struct prometheus_metrics * prometheus_metrics_create(
    char **label_names,
    char **label_values,
    int    label_count);

PROMETHEUS_API void prometheus_metrics_destroy(
    struct prometheus_metrics *metrics);

PROMETHEUS_API int prometheus_metrics_scrape(
    struct prometheus_metrics *metrics,
    char                      *buffer,
    int                        buffer_size);


PROMETHEUS_API struct prometheus_counter * prometheus_metrics_create_counter(
    struct prometheus_metrics *metrics,
    const char                *name,
    const char                *help);

PROMETHEUS_API void prometheus_counter_destroy(
    struct prometheus_metrics *metrics,
    struct prometheus_counter *counter);

PROMETHEUS_API struct prometheus_counter_series * prometheus_counter_create_series(
    struct prometheus_counter *counter,
    const char               **label_names,
    const char               **label_values,
    int                        num_labels);

PROMETHEUS_API void prometheus_counter_destroy_series(
    struct prometheus_counter        *counter,
    struct prometheus_counter_series *series);

PROMETHEUS_API struct prometheus_counter_instance * prometheus_counter_series_create_instance(
    struct prometheus_counter_series *series);

PROMETHEUS_API void prometheus_counter_series_destroy_instance(
    struct prometheus_counter_series   *series,
    struct prometheus_counter_instance *instance);

static inline void
prometheus_counter_increment(struct prometheus_counter_instance *instance)
{
    instance->value++;
} /* prometheus_counter_instance_increment */

static inline void
prometheus_counter_add(
    struct prometheus_counter_instance *instance,
    uint64_t                            value)
{
    instance->value += value;
} /* prometheus_counter_instance_add */

PROMETHEUS_API struct prometheus_gauge * prometheus_metrics_create_gauge(
    struct prometheus_metrics *metrics,
    const char                *name,
    const char                *help);

PROMETHEUS_API void
prometheus_gauge_destroy(
    struct prometheus_metrics *metrics,
    struct prometheus_gauge   *gauge);

PROMETHEUS_API struct prometheus_gauge_series * prometheus_gauge_create_series(
    struct prometheus_gauge *gauge,
    const char             **label_names,
    const char             **label_values,
    int                      num_labels);

PROMETHEUS_API void
prometheus_gauge_destroy_series(
    struct prometheus_gauge        *gauge,
    struct prometheus_gauge_series *series);

PROMETHEUS_API struct prometheus_gauge_instance * prometheus_gauge_series_create_instance(
    struct prometheus_gauge_series *series);

PROMETHEUS_API void prometheus_gauge_series_destroy_instance(
    struct prometheus_gauge_series   *series,
    struct prometheus_gauge_instance *instance);

static inline void
prometheus_gauge_set(
    struct prometheus_gauge_instance *instance,
    int64_t                           value)
{
    instance->value = value;
} /* prometheus_gauge_instance_set */

static inline void
prometheus_gauge_add(
    struct prometheus_gauge_instance *instance,
    int64_t                           value)
{
    instance->value += value;
} /* prometheus_gauge_instance_add */


PROMETHEUS_API struct prometheus_histogram * prometheus_metrics_create_histogram_exponential(
    struct prometheus_metrics *metrics,
    const char                *name,
    const char                *help,
    uint64_t                   count);

PROMETHEUS_API struct prometheus_histogram * prometheus_metrics_create_histogram_linear(
    struct prometheus_metrics *metrics,
    const char                *name,
    const char                *help,
    uint64_t                   start,
    uint64_t                   increment,
    uint64_t                   count);

/*
 * Create a time histogram. Samples are taken with a stopwatch (see
 * prometheus_stopwatch below) and bucketed in raw ticks; bucket boundaries
 * (le) and the sum are reported in nanoseconds at scrape time. `count` is
 * the number of power-of-two tick buckets (~33 spans nanoseconds to ~1s).
 *
 * The first call initializes a process-wide stopwatch context.
 */
PROMETHEUS_API struct prometheus_histogram * prometheus_metrics_create_histogram_time(
    struct prometheus_metrics *metrics,
    const char                *name,
    const char                *help,
    uint64_t                   count);

PROMETHEUS_API void prometheus_histogram_destroy(
    struct prometheus_metrics   *metrics,
    struct prometheus_histogram *histogram);


PROMETHEUS_API struct prometheus_histogram_series * prometheus_histogram_create_series(
    struct prometheus_histogram *base,
    const char                 **label_names,
    const char                 **label_values,
    int                          num_labels);

PROMETHEUS_API void prometheus_histogram_destroy_series(
    struct prometheus_histogram        *histogram,
    struct prometheus_histogram_series *series);


PROMETHEUS_API struct prometheus_histogram_instance * prometheus_histogram_series_create_instance(
    struct prometheus_histogram_series *series);

PROMETHEUS_API void prometheus_histogram_series_destroy_instance(
    struct prometheus_histogram_series   *series,
    struct prometheus_histogram_instance *instance);

static inline void
prometheus_histogram_sample(
    struct prometheus_histogram_instance *instance,
    int64_t                               value)
{
    uint64_t i;

    if (instance->type == PROMETHEUS_HISTOGRAM_EXPONENTIAL) {
        i = prometheus_log2((uint64_t) value);
    } else {
        i = (value - instance->start) / instance->increment;
    }

    if (i >= instance->num_buckets) {
        i = instance->num_buckets - 1;
    }

    instance->buckets[i]++;

    instance->sum += value;
    instance->count++;
} /* prometheus_histogram_instance_sample */

/*
 * Time histogram support.
 *
 * A prometheus_stopwatch is a caller-owned timing handle. Embed it in the
 * context that lives for the duration of the operation being timed (no heap
 * allocation, no locks), start it at the beginning, and sample it into a time
 * histogram instance at the end:
 *
 *   struct prometheus_stopwatch sw;
 *   prometheus_stopwatch_start(&sw);
 *   ... operation ...
 *   prometheus_time_histogram_sample(instance, &sw);
 */
struct prometheus_stopwatch {
    struct stopwatch sw;
};

/*
 * Process-wide stopwatch context. Initialized (once) by
 * prometheus_metrics_create_histogram_time(); read-only thereafter, so the
 * start/sample hot path takes no locks.
 */
extern PROMETHEUS_API struct stopwatch_context prometheus_stopwatch_ctx;

static inline void
prometheus_stopwatch_start(struct prometheus_stopwatch *sw)
{
    stopwatch_start(&prometheus_stopwatch_ctx, &sw->sw);
} /* prometheus_stopwatch_start */

/* Elapsed nanoseconds since the stopwatch was started, for callers that need
 * the value directly (e.g. logging) in addition to histogram sampling.
 */
static inline uint64_t
prometheus_stopwatch_elapsed_ns(struct prometheus_stopwatch *sw)
{
    return stopwatch_elapsed_ns(&prometheus_stopwatch_ctx, &sw->sw);
} /* prometheus_stopwatch_elapsed_ns */

static inline void
prometheus_time_histogram_sample(
    struct prometheus_histogram_instance *instance,
    struct prometheus_stopwatch          *sw)
{
    uint64_t ticks = stopwatch_read_ticks(&prometheus_stopwatch_ctx, &sw->sw);

    /* Same power-of-two bucketing as EXPONENTIAL, but on raw ticks.
     * __builtin_clzll(0) is undefined, so a zero-tick delta maps to bucket 0.
     */
    uint64_t i = prometheus_log2(ticks);

    if (i >= instance->num_buckets) {
        i = instance->num_buckets - 1;
    }

    instance->buckets[i]++;
    instance->sum += ticks;
    instance->count++;
} /* prometheus_time_histogram_sample */

