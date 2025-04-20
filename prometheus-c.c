
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <math.h>
#include "prometheus-c.h"

#define list_append(head, add)  \
        do { \
            if (head) {  \
                (add)->prev        = (head)->prev;  \
                (head)->prev->next = (add);  \
                (head)->prev       = (add);  \
                (add)->next        = NULL; \
            } else {  \
                (head)       = (add);  \
                (head)->prev = (head);  \
                (head)->next = NULL;   \
            } \
        } while (0)

#define list_delete(head, del) \
        do { \
            if ((del)->prev == (del)) { \
                (head) = NULL; \
            } else if ((del) == (head)) { \
                (del)->next->prev = (del)->prev;  \
                (head)            = (del)->next;  \
            } else {  \
                (del)->prev->next = (del)->next;   \
                if ((del)->next) {  \
                    (del)->next->prev = (del)->prev;   \
                } else {  \
                    (head)->prev = (del)->prev; \
                }  \
            } \
        } while (0)

#define list_foreach(head, cur) \
        for (cur = head; cur; cur = cur->next)

struct prometheus_metric_base {
    char *name;
    char *help;
    char  type[16];
};

struct prometheus_series_base {
    char **label_names;
    char **label_values;
    int    label_count;
};

struct prometheus_counter_handle {
    struct prometheus_counter_instance counter;
    struct prometheus_counter_handle  *prev;
    struct prometheus_counter_handle  *next;
} __attribute__((aligned(64)));

struct prometheus_counter_series {
    struct prometheus_series_base     base;
    pthread_mutex_t                   lock;
    struct prometheus_counter_handle *head;
    struct prometheus_counter_series *prev;
    struct prometheus_counter_series *next;
};


struct prometheus_counter {
    struct prometheus_metric_base     base;
    pthread_mutex_t                   lock;
    struct prometheus_counter_series *series;
    struct prometheus_counter        *prev;
    struct prometheus_counter        *next;
};

struct prometheus_gauge_handle {
    struct prometheus_gauge_instance gauge;
    struct prometheus_gauge_handle  *prev;
    struct prometheus_gauge_handle  *next;
} __attribute__((aligned(64)));

struct prometheus_gauge_series {
    struct prometheus_series_base   base;
    pthread_mutex_t                 lock;
    struct prometheus_gauge_handle *head;
    struct prometheus_gauge_series *prev;
    struct prometheus_gauge_series *next;
};

struct prometheus_gauge {
    struct prometheus_metric_base   base;
    pthread_mutex_t                 lock;
    struct prometheus_gauge_series *series;
    struct prometheus_gauge        *prev;
    struct prometheus_gauge        *next;
};

struct prometheus_histogram_handle {
    struct prometheus_histogram_instance histogram;
    struct prometheus_histogram_handle  *prev;
    struct prometheus_histogram_handle  *next;
} __attribute__((aligned(64)));

struct prometheus_histogram_series {
    struct prometheus_series_base       base;
    pthread_mutex_t                     lock;
    uint64_t                           *buckets;
    enum prometheus_histogram_type type;
    uint64_t                            num_buckets;
    uint64_t                            start;
    uint64_t                            increment;
    struct prometheus_histogram_handle *head;
    struct prometheus_histogram_series *prev;
    struct prometheus_histogram_series *next;
};

struct prometheus_histogram {
    struct prometheus_metric_base       base;
    pthread_mutex_t                     lock;
    struct prometheus_histogram_series *series;
    struct prometheus_histogram        *prev;
    struct prometheus_histogram        *next;
    enum prometheus_histogram_type type;
    uint64_t                            count;
    uint64_t                            start;
    uint64_t                            increment;
};

struct prometheus_metrics {
    struct prometheus_counter   *counters;
    struct prometheus_gauge     *gauges;
    struct prometheus_histogram *histograms;
    char                       **label_names;
    char                       **label_values;
    int                          label_count;
    pthread_mutex_t              lock;
};

static inline int
prometheus_string_legal_name(const char *str)
{
    const char *ch;

    if (!str) {
        return 0;
    }

    for (ch = str; *ch; ch++) {
        if (!isalnum(*ch) && *ch != '_') {
            return 0;
        }
    }

    return 1;
} /* prometheus_string_legal */

static inline int
prometheus_string_legal_value(const char *str)
{
    const char *ch;

    if (!str) {
        return 0;
    }

    for (ch = str; *ch; ch++) {
        if (*ch == '"') {
            return 0;
        }
    }

    return 1;
} /* prometheus_string_legal_help */

static inline void *
prometheus_calloc(
    int    n,
    size_t size)
{
    void *ptr = calloc(n, size);

    if (!ptr) {
        abort();
    }

    return ptr;
} /* prometheus_malloc */

static inline char *
prometheus_strdup(const char *str)
{
    int   len = strlen(str);

    char *ptr = malloc(len + 1);

    if (!ptr) {
        abort();
    }

    memcpy(ptr, str, len + 1);

    return ptr;
} /* prometheus_strdup */

struct prometheus_metrics *
prometheus_metrics_create(
    char **label_names,
    char **label_values,
    int    label_count)
{
    struct prometheus_metrics *metrics;

    metrics = prometheus_calloc(1, sizeof(*metrics));

    metrics->label_count  = label_count;
    metrics->label_names  = prometheus_calloc(label_count, sizeof(char *));
    metrics->label_values = prometheus_calloc(label_count, sizeof(char *));

    for (int i = 0; i < label_count; i++) {
        metrics->label_names[i]  = prometheus_strdup(label_names[i]);
        metrics->label_values[i] = prometheus_strdup(label_values[i]);
    }

    pthread_mutex_init(&metrics->lock, NULL);

    return metrics;
} /* prometheus_metrics_create */

static void
prometheus_metric_base_destroy(struct prometheus_metric_base *base)
{
    free(base->name);
    free(base->help);
} /* prometheus_metric_base_destroy */

static void
prometheus_series_base_destroy(struct prometheus_series_base *base)
{
    int i;

    for (i = 0; i < base->label_count; i++) {
        free(base->label_names[i]);
        free(base->label_values[i]);
    }

    free(base->label_names);
    free(base->label_values);
} /* prometheus_series_base_destroy */

static void
prometheus_counter_destroy(struct prometheus_counter *counter)
{
    struct prometheus_counter_series *series;
    struct prometheus_counter_handle *hdl;

    while (counter->series) {
        series = counter->series;
        list_delete(counter->series, series);

        while (series->head) {
            hdl = series->head;
            list_delete(series->head, hdl);


            free(hdl);
        }

        pthread_mutex_destroy(&series->lock);

        prometheus_series_base_destroy(&series->base);

        free(series);
    }

    pthread_mutex_destroy(&counter->lock);

    prometheus_metric_base_destroy(&counter->base);

    free(counter);
} /* prometheus_counter_destroy */


static void
prometheus_gauge_destroy(struct prometheus_gauge *gauge)
{
    struct prometheus_gauge_series *series;
    struct prometheus_gauge_handle *hdl;

    while (gauge->series) {
        series = gauge->series;
        list_delete(gauge->series, series);

        while (series->head) {
            hdl = series->head;
            list_delete(series->head, hdl);

            free(hdl);
        }

        pthread_mutex_destroy(&series->lock);

        prometheus_series_base_destroy(&series->base);
        free(series);
    }

    pthread_mutex_destroy(&gauge->lock);

    prometheus_metric_base_destroy(&gauge->base);

    free(gauge);
} /* prometheus_gauge_destroy */

static void
prometheus_histogram_destroy(struct prometheus_histogram *histogram)
{
    struct prometheus_histogram_series *series;
    struct prometheus_histogram_handle *hdl;

    while (histogram->series) {
        series = histogram->series;
        list_delete(histogram->series, series);

        while (series->head) {
            hdl = series->head;
            list_delete(series->head, hdl);

            free(hdl->histogram.buckets);

            free(hdl);
        }

        pthread_mutex_destroy(&series->lock);

        prometheus_series_base_destroy(&series->base);

        free(series->buckets);

        free(series);
    }

    pthread_mutex_destroy(&histogram->lock);

    prometheus_metric_base_destroy(&histogram->base);

    free(histogram);
} /* prometheus_histogram_destroy */

void
prometheus_metrics_destroy(struct prometheus_metrics *metrics)
{
    struct prometheus_counter   *counter;
    struct prometheus_gauge     *gauge;
    struct prometheus_histogram *histogram;
    int                          i;

    while (metrics->counters) {
        counter = metrics->counters;
        list_delete(metrics->counters, counter);

        prometheus_counter_destroy(counter);
    }

    while (metrics->gauges) {
        gauge = metrics->gauges;
        list_delete(metrics->gauges, gauge);

        prometheus_gauge_destroy(gauge);
    }

    while (metrics->histograms) {
        histogram = metrics->histograms;
        list_delete(metrics->histograms, histogram);

        prometheus_histogram_destroy(histogram);
    }

    for (i = 0; i < metrics->label_count; i++) {
        free(metrics->label_names[i]);
        free(metrics->label_values[i]);
    }

    pthread_mutex_destroy(&metrics->lock);

    free(metrics->label_names);
    free(metrics->label_values);
    free(metrics);
} /* prometheus_metrics_destroy */

static inline void
prometheus_metric_base_init(
    struct prometheus_metric_base *base,
    const char                    *name,
    const char                    *help,
    const char                    *type)
{
    base->name = prometheus_strdup(name);
    base->help = prometheus_strdup(help);
    snprintf(base->type, sizeof(base->type), "%s", type);
} /* prometheus_metric_base_init */

static inline void
prometheus_series_base_init(
    struct prometheus_series_base *base,
    int                            num_labels,
    const char                   **label_names,
    const char                   **label_values)
{
    base->label_count  = num_labels;
    base->label_names  = prometheus_calloc(num_labels, sizeof(char *));
    base->label_values = prometheus_calloc(num_labels, sizeof(char *));

    for (int i = 0; i < num_labels; i++) {
        base->label_names[i]  = prometheus_strdup(label_names[i]);
        base->label_values[i] = prometheus_strdup(label_values[i]);
    }
} /* prometheus_series_base_init */

static inline char *
prometheus_metrics_emit_base(
    char                          *bp,
    struct prometheus_metric_base *base)
{
    bp += sprintf(bp, "# HELP %s %s\n", base->name, base->help);
    bp += sprintf(bp, "# TYPE %s %s\n", base->name, base->type);

    return bp;
} /* prometheus_metrics_emit_base */

static inline char *
prometheus_metrics_emit_series_base(
    char                          *bp,
    struct prometheus_metrics     *metrics,
    const char                    *metric_suffix,
    struct prometheus_metric_base *metric_base,
    struct prometheus_series_base *series_base)
{
    int i;

    bp += sprintf(bp, "%s%s{", metric_base->name, metric_suffix);

    for (i = 0; i < metrics->label_count; i++) {
        bp += sprintf(bp, "%s=\"%s\",", metrics->label_names[i], metrics->label_values[i]);
    }

    for (i = 0; i < series_base->label_count; i++) {
        bp += sprintf(bp, "%s=\"%s\",", series_base->label_names[i], series_base->label_values[i]);
    }

    if (*(bp - 1) == ',') {
        bp--;
        *bp = '\0';
    }

    bp += sprintf(bp, "} ");

    return bp;
} /* prometheus_metrics_emit_series_base */

int
prometheus_metrics_scrape(
    struct prometheus_metrics *metrics,
    char                      *buffer,
    int                        buffer_size)
{
    struct prometheus_counter          *counter;
    struct prometheus_counter_series   *counter_series;
    struct prometheus_counter_handle   *counter_hdl;
    struct prometheus_gauge            *gauge;
    struct prometheus_gauge_series     *gauge_series;
    struct prometheus_gauge_handle     *gauge_hdl;
    struct prometheus_histogram        *histogram;
    struct prometheus_histogram_series *histogram_series;
    struct prometheus_histogram_handle *histogram_hdl;
    char                                histogram_suffix[64];
    uint64_t                            value, sum, total;
    int                                 i;
    char                               *bp = buffer;

    *bp = '\0';

    if (!metrics || !buffer || !buffer_size) {
        return -1;
    }


    pthread_mutex_lock(&metrics->lock);

    list_foreach(metrics->counters, counter)
    {

        pthread_mutex_lock(&counter->lock);

        bp = prometheus_metrics_emit_base(bp, &counter->base);

        list_foreach(counter->series, counter_series)
        {
            pthread_mutex_lock(&counter_series->lock);

            value = 0;

            list_foreach(counter_series->head, counter_hdl)
            {
                value += counter_hdl->counter.value;

            }

            bp = prometheus_metrics_emit_series_base(bp, metrics, "", &counter->base, &counter_series->base);

            bp += sprintf(bp, "%lu\n", value);

            pthread_mutex_unlock(&counter_series->lock);
        }

        *bp++ = '\n';
        *bp   = '\0';

        pthread_mutex_unlock(&counter->lock);
    }

    list_foreach(metrics->gauges, gauge)
    {
        pthread_mutex_lock(&gauge->lock);

        bp = prometheus_metrics_emit_base(bp, &gauge->base);

        list_foreach(gauge->series, gauge_series)
        {
            pthread_mutex_lock(&gauge_series->lock);

            value = 0;

            list_foreach(gauge_series->head, gauge_hdl)
            {
                value += gauge_hdl->gauge.value;
            }

            bp = prometheus_metrics_emit_series_base(bp, metrics, "", &gauge->base, &gauge_series->base);

            bp += sprintf(bp, "%lu\n", value);

            pthread_mutex_unlock(&gauge_series->lock);

        }

        pthread_mutex_unlock(&gauge->lock);
    }

    list_foreach(metrics->histograms, histogram)
    {
        pthread_mutex_lock(&histogram->lock);

        bp = prometheus_metrics_emit_base(bp, &histogram->base);

        list_foreach(histogram->series, histogram_series)
        {
            pthread_mutex_lock(&histogram_series->lock);

            sum   = 0;
            total = 0;

            list_foreach(histogram_series->head, histogram_hdl)
            {
                sum   += histogram_hdl->histogram.sum;
                total += histogram_hdl->histogram.count;
            }

            for (i = 0; i < histogram->count; i++) {

                histogram_series->buckets[i] = 0;

                list_foreach(histogram_series->head, histogram_hdl)
                {
                    histogram_series->buckets[i] += histogram_hdl->histogram.buckets[i];
                }

                if (i + 1 < histogram->count) {
                    if (histogram->type == PROMETHEUS_HISTOGRAM_EXPONENTIAL) {
                        snprintf(histogram_suffix, sizeof(histogram_suffix), "_bucket{le=\"%lu\"}", (1UL << (i + 1)));
                    } else {
                        snprintf(histogram_suffix, sizeof(histogram_suffix), "_bucket{le=\"%lu\"}", histogram->start +
                                 histogram->increment * (i + 1));
                    }
                } else {
                    snprintf(histogram_suffix, sizeof(histogram_suffix), "_bucket{le=\"+Inf\"}");
                }

                bp = prometheus_metrics_emit_series_base(bp, metrics, histogram_suffix, &histogram->base, &
                                                         histogram_series->base);

                bp += sprintf(bp, "%lu\n", histogram_series->buckets[i]);

                fprintf(stderr, "%s\n", buffer);
            }

            bp = prometheus_metrics_emit_series_base(bp, metrics, "_sum", &histogram->base, &histogram_series->base);

            bp += sprintf(bp, "%lu\n", sum);

            bp = prometheus_metrics_emit_series_base(bp, metrics, "_count", &histogram->base, &histogram_series->base);

            bp += sprintf(bp, "%lu\n", total);

            pthread_mutex_unlock(&histogram_series->lock);
        }

        *bp++ = '\n';
        *bp   = '\0';

        pthread_mutex_unlock(&histogram->lock);
    }

    pthread_mutex_unlock(&metrics->lock);

    return bp - buffer;
} /* prometheus_metrics_scrape */

struct prometheus_counter *
prometheus_metrics_add_counter(
    struct prometheus_metrics *metrics,
    const char                *name,
    const char                *help)
{
    struct prometheus_counter *counter;

    if (!prometheus_string_legal_name(name)) {
        return NULL;
    }

    pthread_mutex_lock(&metrics->lock);

    counter = prometheus_calloc(1, sizeof(*counter));

    prometheus_metric_base_init(&counter->base, name, help, "counter");

    pthread_mutex_init(&counter->lock, NULL);

    list_append(metrics->counters, counter);

    pthread_mutex_unlock(&metrics->lock);

    return counter;
} /* prometheus_metrics_add_counter */

struct prometheus_counter_series *
prometheus_counter_add_series(
    struct prometheus_counter *counter,
    const char               **label_names,
    const char               **label_values,
    int                        num_labels)
{
    struct prometheus_counter_series *series;
    int                               i;

    for (i = 0; i < num_labels; i++) {
        if (!prometheus_string_legal_name(label_names[i])) {
            return NULL;
        }

        if (!prometheus_string_legal_value(label_values[i])) {
            return NULL;
        }
    }

    pthread_mutex_lock(&counter->lock);

    series = prometheus_calloc(1, sizeof(*series));

    prometheus_series_base_init(&series->base, num_labels, label_names, label_values);

    pthread_mutex_init(&series->lock, NULL);

    list_append(counter->series, series);

    pthread_mutex_unlock(&counter->lock);

    return series;
} /* prometheus_counter_add_series */

struct prometheus_counter_instance *
prometheus_counter_series_create_instance(struct prometheus_counter_series *series)
{
    struct prometheus_counter_handle *hdl;

    pthread_mutex_lock(&series->lock);

    hdl = prometheus_calloc(1, sizeof(*hdl));

    list_append(series->head, hdl);

    pthread_mutex_unlock(&series->lock);

    return &hdl->counter;
} /* prometheus_counter_create_instance */

struct prometheus_gauge *
prometheus_metrics_add_gauge(
    struct prometheus_metrics *metrics,
    const char                *name,
    const char                *help)
{
    struct prometheus_gauge *gauge;

    if (!prometheus_string_legal_name(name)) {
        return NULL;
    }

    pthread_mutex_lock(&metrics->lock);

    gauge = prometheus_calloc(1, sizeof(*gauge));

    prometheus_metric_base_init(&gauge->base, name, help, "gauge");

    pthread_mutex_init(&gauge->lock, NULL);

    list_append(metrics->gauges, gauge);

    pthread_mutex_unlock(&metrics->lock);

    return gauge;
} /* prometheus_metrics_add_gauge */

struct prometheus_gauge_series *
prometheus_gauge_add_series(
    struct prometheus_gauge *gauge,
    const char             **label_names,
    const char             **label_values,
    int                      num_labels)
{
    struct prometheus_gauge_series *series;
    int                             i;

    for (i = 0; i < num_labels; i++) {
        if (!prometheus_string_legal_name(label_names[i])) {
            return NULL;
        }

        if (!prometheus_string_legal_value(label_values[i])) {
            return NULL;
        }
    }

    pthread_mutex_lock(&gauge->lock);

    series = prometheus_calloc(1, sizeof(*series));

    prometheus_series_base_init(&series->base, num_labels, label_names, label_values);

    pthread_mutex_init(&series->lock, NULL);

    list_append(gauge->series, series);

    pthread_mutex_unlock(&gauge->lock);

    return series;
} /* prometheus_gauge_add_series */

struct prometheus_gauge_instance *
prometheus_gauge_series_create_instance(struct prometheus_gauge_series *series)
{
    struct prometheus_gauge_handle *hdl;

    pthread_mutex_lock(&series->lock);

    hdl = prometheus_calloc(1, sizeof(*hdl));

    list_append(series->head, hdl);

    pthread_mutex_unlock(&series->lock);

    return &hdl->gauge;
} /* prometheus_gauge_series_create_instance */


struct prometheus_histogram *
prometheus_metrics_add_histogram_exponential(
    struct prometheus_metrics *metrics,
    const char                *name,
    const char                *help,
    uint64_t                   count)
{
    struct prometheus_histogram *histogram;

    if (!prometheus_string_legal_name(name)) {
        return NULL;
    }

    pthread_mutex_lock(&metrics->lock);

    histogram = prometheus_calloc(1, sizeof(*histogram));

    prometheus_metric_base_init(&histogram->base, name, help, "histogram");

    histogram->type  = PROMETHEUS_HISTOGRAM_EXPONENTIAL;
    histogram->count = count;

    pthread_mutex_init(&histogram->lock, NULL);

    list_append(metrics->histograms, histogram);

    pthread_mutex_unlock(&metrics->lock);

    return histogram;
} /* prometheus_metrics_add_histogram */


struct prometheus_histogram *
prometheus_metrics_add_histogram_linear(
    struct prometheus_metrics *metrics,
    const char                *name,
    const char                *help,
    uint64_t                   start,
    uint64_t                   increment,
    uint64_t                   count)
{
    struct prometheus_histogram *histogram;

    if (!prometheus_string_legal_name(name)) {
        return NULL;
    }

    pthread_mutex_lock(&metrics->lock);

    histogram = prometheus_calloc(1, sizeof(*histogram));

    prometheus_metric_base_init(&histogram->base, name, help, "histogram");

    histogram->type      = PROMETHEUS_HISTOGRAM_LINEAR;
    histogram->count     = count;
    histogram->start     = start;
    histogram->increment = increment;

    pthread_mutex_init(&histogram->lock, NULL);

    list_append(metrics->histograms, histogram);

    pthread_mutex_unlock(&metrics->lock);

    return histogram;
} /* prometheus_metrics_add_histogram */

struct prometheus_histogram_series *
prometheus_histogram_add_series(
    struct prometheus_histogram *histogram,
    const char                 **label_names,
    const char                 **label_values,
    int                          num_labels)
{
    struct prometheus_histogram_series *series;
    int                                 i;

    for (i = 0; i < num_labels; i++) {
        if (!prometheus_string_legal_name(label_names[i])) {
            return NULL;
        }

        if (!prometheus_string_legal_value(label_values[i])) {
            return NULL;
        }
    }

    pthread_mutex_lock(&histogram->lock);

    series = prometheus_calloc(1, sizeof(*series));

    prometheus_series_base_init(&series->base, num_labels, label_names, label_values);

    series->buckets     = prometheus_calloc(histogram->count, sizeof(uint64_t));
    series->type        = histogram->type;
    series->num_buckets = histogram->count;
    series->start       = histogram->start;
    series->increment   = histogram->increment;

    pthread_mutex_init(&series->lock, NULL);

    list_append(histogram->series, series);

    pthread_mutex_unlock(&histogram->lock);

    return series;
} /* prometheus_histogram_add_series */

struct prometheus_histogram_instance *
prometheus_histogram_series_create_instance(struct prometheus_histogram_series *series)
{
    struct prometheus_histogram_handle *hdl;

    pthread_mutex_lock(&series->lock);

    hdl = prometheus_calloc(1, sizeof(*hdl));

    hdl->histogram.buckets     = prometheus_calloc(series->num_buckets, sizeof(uint64_t));
    hdl->histogram.type        = series->type;
    hdl->histogram.num_buckets = series->num_buckets;
    hdl->histogram.start       = series->start;
    hdl->histogram.increment   = series->increment;

    list_append(series->head, hdl);

    pthread_mutex_unlock(&series->lock);

    return &hdl->histogram;
} /* prometheus_histogram_series_create_instance */
