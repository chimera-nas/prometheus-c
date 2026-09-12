// SPDX-FileCopyrightText: 2026 Ben Jarvis
// SPDX-License-Identifier: LGPL-2.1-only
#include <assert.h>
#include <string.h>
#include "prometheus-c.h"
#ifndef _WIN32
#include <pthread.h>
#endif /* ifndef _WIN32 */
#ifdef NDEBUG
#error Tests require assertions
#endif /* ifdef NDEBUG */
static struct prometheus_counter_series *series;
static void
work(void)
{
    for (int n = 0; n < 1000; ++n) {
        struct prometheus_counter_instance *instance = prometheus_counter_series_create_instance(series);
        assert((uintptr_t) instance % 64 == 0);
        prometheus_counter_add(instance, 100);
        prometheus_counter_series_destroy_instance(series, instance);
    }
} /* work */
#ifdef _WIN32
static DWORD WINAPI
worker(void *arg)
{
    (void) arg;
    work();
    return 0;
} /* worker */
#else  /* ifdef _WIN32 */
static void *
worker(void *arg)
{
    (void) arg;
    work();
    return NULL;
} /* worker */
#endif /* ifdef _WIN32 */
int
main(void)
{
    struct prometheus_metrics *metrics = prometheus_metrics_create(NULL, NULL, 0);
    struct prometheus_counter *counter = prometheus_metrics_create_counter(metrics, "concurrent_counter",
                                                                           "Concurrent instances");
    char                       buffer[16384];

    series = prometheus_counter_create_series(counter, NULL, NULL, 0);
#ifdef _WIN32
    HANDLE                     threads[4];
    for (int i = 0; i < 4; ++i) {
        threads[i] = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        assert(threads[i]);
    }
    for (int i = 0; i < 4; ++i) {
        assert(WaitForSingleObject(threads[i], INFINITE) == WAIT_OBJECT_0);
        CloseHandle(threads[i]);
    }
#else  /* ifdef _WIN32 */
    pthread_t                             threads[4];
    for (int i = 0; i < 4; ++i) {
        assert(pthread_create(&threads[i], NULL, worker, NULL) == 0);
    }
    for (int i = 0; i < 4; ++i) {
        assert(pthread_join(threads[i], NULL) == 0);
    }
#endif /* ifdef _WIN32 */
    struct prometheus_histogram          *histogram = prometheus_metrics_create_histogram_exponential(metrics,
                                                                                                      "wide",
                                                                                                      "64-bit boundaries",
                                                                                                      64);
    struct prometheus_histogram_series   *histogram_series = prometheus_histogram_create_series(histogram, NULL, NULL, 0
                                                                                                );
    struct prometheus_histogram_instance *instance = prometheus_histogram_series_create_instance(
        histogram_series);
    prometheus_histogram_sample(instance, INT64_C(1) << 40);
    assert(prometheus_metrics_scrape(metrics, buffer, sizeof(buffer)) > 0);
    assert(strstr(buffer, "concurrent_counter{} 400000\n"));
    assert(strstr(buffer, "wide_bucket{le=\"2199023255552\"} 1\n"));
    assert(prometheus_log2(0) == 0);
    assert(prometheus_log2(1) == 0);
    assert(prometheus_log2(2) == 1);
    assert(prometheus_log2(UINT64_C(1) << 63) == 63);
    prometheus_metrics_destroy(metrics);
    return 0;
} /* main */
