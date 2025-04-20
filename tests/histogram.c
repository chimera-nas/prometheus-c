
#include <stdio.h>
#include <stdlib.h>
#include "prometheus-c.h"

int
main(
    int    argc,
    char **argv)
{
    struct prometheus_metrics            *metrics;
    struct prometheus_histogram          *histogram1, *histogram2;
    struct prometheus_histogram_series   *series11, *series12, *series21, *series22;
    struct prometheus_histogram_instance *instance11, *instance12, *instance21, *instance22;
    char                                 *buffer;

    buffer = malloc(1024 * 1024);

    metrics = prometheus_metrics_create((char *[]) { "global" }, (char *[]) { "root" }, 1);

    histogram1 = prometheus_metrics_create_histogram_exponential(metrics, "test_histogram1", "Test histogram1", 16);
    histogram2 = prometheus_metrics_create_histogram_linear(metrics, "test_histogram2", "Test histogram2", 10, 10, 10);

    series11 = prometheus_histogram_create_series(histogram1,
                                                  (const char *[]) { "test" }, (const char *[]) { "test1" },
                                                  1);
    series12 = prometheus_histogram_create_series(histogram1,
                                                  (const char *[]) { "test" }, (const char *[]) { "test2" },
                                                  1);
    series21 = prometheus_histogram_create_series(histogram2,
                                                  (const char *[]) { "test" }, (const char *[]) { "test1" },
                                                  1);
    series22 = prometheus_histogram_create_series(histogram2,
                                                  (const char *[]) { "test" }, (const char *[]) { "test2" },
                                                  1);

    instance11 = prometheus_histogram_series_create_instance(series11);
    instance12 = prometheus_histogram_series_create_instance(series12);
    instance21 = prometheus_histogram_series_create_instance(series21);
    instance22 = prometheus_histogram_series_create_instance(series22);

    prometheus_histogram_sample(instance11, 100);
    prometheus_histogram_sample(instance12, 200);
    prometheus_histogram_sample(instance21, 300);
    prometheus_histogram_sample(instance22, 400);

    prometheus_histogram_sample(instance11, 11);
    prometheus_histogram_sample(instance12, 21);
    prometheus_histogram_sample(instance21, 31);
    prometheus_histogram_sample(instance22, 41);

    prometheus_metrics_scrape(metrics, buffer, sizeof(buffer));
    printf("%s\n", buffer);

    prometheus_metrics_destroy(metrics);

    free(buffer);

    return 0;
} /* main */