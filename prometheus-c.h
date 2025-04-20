#pragma once

#include <stdint.h>
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

struct prometheus_metrics * prometheus_metrics_create(
    char **label_names,
    char **label_values,
    int    label_count);

void prometheus_metrics_destroy(
    struct prometheus_metrics *metrics);

int prometheus_metrics_scrape(
    struct prometheus_metrics *metrics,
    char                      *buffer,
    int                        buffer_size);


struct prometheus_counter * prometheus_metrics_create_counter(
    struct prometheus_metrics *metrics,
    const char                *name,
    const char                *help);

void prometheus_counter_destroy(
    struct prometheus_metrics *metrics,
    struct prometheus_counter *counter);

struct prometheus_counter_series * prometheus_counter_create_series(
    struct prometheus_counter *counter,
    const char               **label_names,
    const char               **label_values,
    int                        num_labels);

void prometheus_counter_destroy_series(
    struct prometheus_counter        *counter,
    struct prometheus_counter_series *series);

struct prometheus_counter_instance * prometheus_counter_series_create_instance(
    struct prometheus_counter_series *series);

void prometheus_counter_series_destroy_instance(
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

struct prometheus_gauge * prometheus_metrics_create_gauge(
    struct prometheus_metrics *metrics,
    const char                *name,
    const char                *help);


struct prometheus_gauge_series * prometheus_gauge_create_series(
    struct prometheus_gauge *gauge,
    const char             **label_names,
    const char             **label_values,
    int                      num_labels);


struct prometheus_gauge_instance * prometheus_gauge_series_create_instance(
    struct prometheus_gauge_series *series);

void prometheus_gauge_series_destroy_instance(
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


struct prometheus_histogram * prometheus_metrics_create_histogram_exponential(
    struct prometheus_metrics *metrics,
    const char                *name,
    const char                *help,
    uint64_t                   count);

struct prometheus_histogram * prometheus_metrics_create_histogram_linear(
    struct prometheus_metrics *metrics,
    const char                *name,
    const char                *help,
    uint64_t                   start,
    uint64_t                   increment,
    uint64_t                   count);


struct prometheus_histogram_series * prometheus_histogram_create_series(
    struct prometheus_histogram *base,
    const char                 **label_names,
    const char                 **label_values,
    int                          num_labels);


struct prometheus_histogram_instance * prometheus_histogram_series_create_instance(
    struct prometheus_histogram_series *series);

void prometheus_histogram_series_destroy_instance(
    struct prometheus_histogram_series   *series,
    struct prometheus_histogram_instance *instance);

static inline void
prometheus_histogram_sample(
    struct prometheus_histogram_instance *instance,
    int64_t                               value)
{
    uint64_t i;

    if (instance->type == PROMETHEUS_HISTOGRAM_EXPONENTIAL) {
        i = 63 - __builtin_clzll(value);
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

