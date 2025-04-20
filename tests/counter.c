
#include <stdio.h>
#include "prometheus-c.h"

int
main(
    int    argc,
    char **argv)
{
    struct prometheus_metrics          *metrics;
    struct prometheus_counter          *counter1, *counter2;
    struct prometheus_counter_series   *series11, *series12, *series21, *series22;
    struct prometheus_counter_instance *instance11, *instance12, *instance21, *instance22;
    char                                buffer[4096];

    metrics = prometheus_metrics_create((char *[]) { "global" }, (char *[]) { "root" }, 1);

    counter1 = prometheus_metrics_create_counter(metrics, "test_counter1", "Test counter1");
    counter2 = prometheus_metrics_create_counter(metrics, "test_counter2", "Test counter2");

    series11 = prometheus_counter_create_series(counter1, (const char *[]) { "test" }, (const char *[]) { "test1" }, 1);
    series12 = prometheus_counter_create_series(counter1, (const char *[]) { "test" }, (const char *[]) { "test2" }, 1);
    series21 = prometheus_counter_create_series(counter2, (const char *[]) { "test" }, (const char *[]) { "test1" }, 1);
    series22 = prometheus_counter_create_series(counter2, (const char *[]) { "test" }, (const char *[]) { "test2" }, 1);

    instance11 = prometheus_counter_series_create_instance(series11);
    instance12 = prometheus_counter_series_create_instance(series12);
    instance21 = prometheus_counter_series_create_instance(series21);
    instance22 = prometheus_counter_series_create_instance(series22);

    prometheus_counter_increment(instance11);
    prometheus_counter_increment(instance12);
    prometheus_counter_increment(instance21);
    prometheus_counter_increment(instance22);

    prometheus_counter_add(instance11, 10);
    prometheus_counter_add(instance12, 20);
    prometheus_counter_add(instance21, 30);
    prometheus_counter_add(instance22, 40);

    prometheus_metrics_scrape(metrics, buffer, sizeof(buffer));
    printf("%s\n", buffer);

    prometheus_metrics_destroy(metrics);

    return 0;
} /* main */