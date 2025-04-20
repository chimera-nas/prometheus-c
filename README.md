# prometheus-c

This is a C library intended to facilitate exposition of Prometheus (or OpenMetrics) style metrics from a C program/daemon.

## Rationale

There is a more mature and more complete prometheus library for C available from digitalocean at https://github.com/digitalocean/prometheus-client-c.

The digitalocean library has pretty complete support for the specification and has a convenient API that is similar to what you will find
in other programming languages prometheus SDKs.   The runtime cost is however correspondingly high, which may be irrelevant or critical to you
depending on the frequency with which your application will need to update metrics.  If you are calculating metrics on operations that occur
at rates of millions per second, it's probably too expensive.  Thousands per second, it's fine.

This library differs in that it is much narrower in scope and very focused on minimizing the performance impact of the metrics tracking.
This comes at the cost of more boilerplate required and restrictions on features.

In particular:

* All metrics and metric series must be allocated in advance and manipulated via handle pointers. This avoids the need to do any string
  manipulation on a per-sample basis.
* The architecture is that a metric may have any number of series, and each series may have any number of handles.
  When the application wants to add a sample to a metric, it does so using a handle.   Handles are _not_ thread safe and can be 
  utilized in several ways:
  * Put a handle instance inside each existing data in the existing application code that is already protected by some kind of
    mutual exclusion scheme.  In this way the metrics are thread-safe without the need for additional locking or atomics
  * Create a private handle for each thread that will manipulate the metric series.  In this way, no locking or atomics are required.
  * Use a single global handle and share it amongst all the threads with application provided mutual exclusion.

  In any case, the metric samples added to each handle are automatically summed at scraping time.  Therefore, the number of handles
  is transparent to prometheus.
* The library currently supports only integer arithmetic.
  * Counters are unsigned 64-bit.
  * Gauges are signed 64-bit.
  * Histograms are unsigned 64-bit and have two flavors:
    * Exponential power of two scale (le=2,le=4,...).  This allows value-to-bucket mapping to be done by single instruction
    * Linear scale (start, start*2, start*3, ...).

## API

### Thread Safety

All the functions related to creating and destroying metrics, metric series, and series instance handles are thread safe.

The metrics scraping API is thread safe.

The metric instance handles themselves are not thread safe.   That is to say, if two threads increment a counter handle at
the same time, the resulting counter value might increment by only one.   This will not cause a crash or anything but it
may result in counters that are not completely accurate.  If this matters to you, either use instance handles in a thread
safe way in your application as described above or protect them yourselves with a lock.

### Global Metrics State

The application should first create a global metrics state once per process:

```c
struct prometheus_metrics *prometheus_metrics_create(
    char **label_names,    // Array of global label names
    char **label_values,   // Array of global label values
    int    label_count);   // Number of global labels
```

The provided labels will be added to all metric series emitted by the library.

This may return NULL if any of the label names or values contain illegal characteres.

Before program exit, the global metrics context should be destroyed:

```c
void prometheus_metrics_destroy(
    struct prometheus_metrics *metrics);
 ```

All previously created metrics, series, and handles that were not previously destroyed explicitly are implicitly
destroyed when the global context is destroyed.  Therefore it is only necessary to explicitly destroy metrics,
series, and handles if they belong to a structure or thread that is being destroyed and you want to avoid leaking them.

The prometheus metrics can be scraped into a provided UTF-8 buffer as follows:

```c
int prometheus_metrics_scrape(
    struct prometheus_metrics *metrics,
    char                      *buffer,    // Output buffer for metrics
    int                        buffer_size);
```

This function will return the length of resulting string that was printed into 'buffer' or -1 if the provided buffer was not large enough.

The metrics scraping process is non-blocking with respect to metrics sampling functions.

The task of serving the scraped metrics string via HTTP or pushing it to a prometheus/OpenMetrics push gateway is left to the user.  However, a couple options from the chimera
project itself include:


https://github.com/chimera-nas/stupid-httpd is a very simple single thread
ed httpd server meant only for serving something like prometheus metrics in an insecure way.

https://github.com/chimera-nas/libevpl is a high performance networking library that includes an httpd server component.

### Counters

Counters are unsigned 64-bit integers that can only increase.

A counter can be created as follows:

```c
struct prometheus_counter *prometheus_metrics_create_counter(
    struct prometheus_metrics *metrics,
    const char                *name,
    const char                *help);
```

The name must be [A-Z-a-z0-9_]+ and the help is an english string describing the metric.

May return NULL if name contains illegal characters.

A counter can be optionally explicitly destroyed as follows
```c
void prometheus_counter_destroy(
    struct prometheus_metrics *metrics,
    struct prometheus_counter *counter);
```

If a counter is destroyed, all series of the counter are implicitly destroyed as well.

If a counter is not explicitly destroyed it will be automatically destroyed when the global context is destroyed.

One or more series of a counter metric can be created as follows:

```c
struct prometheus_counter_series *prometheus_counter_add_series(
    struct prometheus_counter *base,
    const char               **label_names,
    const char               **label_values,
    int                        num_labels);
```

The provided array of label name=value pairs become the series labels, in addition to those provided globally at initialization.

Label names must be [A-Za-z0-9_]+ and label values may be any character except double quote.

May return NULL if any label name or value contains illegal characters.

A counter series may be optionally explicitly destroyed as follows:
```c
void prometheus_counter_destroy_series(
    struct prometheus_counter        *counter,
    struct prometheus_counter_series *series);
```

In order to increment a counter, one or more handle instances must be created:

```c
struct prometheus_counter_instance *prometheus_counter_series_create_instance(
    struct prometheus_counter_series *series);
 ```c

 Handle instances can be optionally explcitly destroyed as follows:
 ```c
void prometheus_counter_series_destroy_instance(
    struct prometheus_counter_series   *series,
    struct prometheus_counter_instance *instance);
```

 The value of the counter becomes the sum of the values of its instance handles.

 The handle instance values can be manipulated as follows:

```c
void prometheus_counter_increment(struct prometheus_counter_instance *instance);
void prometheus_counter_add(struct prometheus_counter_instance *instance, uint64_t value);
```

### Gauges

Gauges are signed 64-bit integers that can increase or decrease.

A gauge can be created as follows:

```c
struct prometheus_gauge *prometheus_metrics_create_gauge(
    struct prometheus_metrics *metrics,
    const char                *name,
    const char                *help);
```

The name must be [A-Z-a-z0-9_]+ and the help is an english string describing the metric.

May return NULL if name contains illegal characters.

A gauge can be optionally explicitly destroyed as follows:
```c
void prometheus_gauge_destroy(
    struct prometheus_metrics *metrics,
    struct prometheus_gauge *gauge);
```

If a gauge is destroyed, all series of the gauge are implicitly destroyed as well.

If a gauge is not explicitly destroyed it will be automatically destroyed when the global context is destroyed.

One or more series of a gauge metric can be created as follows:

```c
struct prometheus_gauge_series *prometheus_gauge_create_series(
    struct prometheus_gauge *gauge,
    const char             **label_names,
    const char             **label_values,
    int                      num_labels);
```

The provided array of label name=value pairs become the series labels, in addition to those provided globally at initialization.

Label names must be [A-Za-z0-9_]+ and label values may be any character except double quote.

May return NULL if any label name or value contains illegal characters.

A gauge series may be optionally explicitly destroyed as follows:
```c
void prometheus_gauge_destroy_series(
    struct prometheus_gauge        *gauge,
    struct prometheus_gauge_series *series);
```

In order to manipulate a gauge, one or more handle instances must be created:

```c
struct prometheus_gauge_instance *prometheus_gauge_series_create_instance(
    struct prometheus_gauge_series *series);
```

Handle instances can be optionally explicitly destroyed as follows:
```c
void prometheus_gauge_series_destroy_instance(
    struct prometheus_gauge_series   *series,
    struct prometheus_gauge_instance *instance);
```

The value of the gauge becomes the sum of the values of its instance handles.

The handle instance values can be manipulated as follows:

```c
void prometheus_gauge_set(struct prometheus_gauge_instance *instance, int64_t value);
void prometheus_gauge_add(struct prometheus_gauge_instance *instance, int64_t value);
```

### Histograms

Histograms are used to track distributions of values. They support two types of bucket configurations:

A histogram can be created with exponential buckets (2, 4, 8, 16, ...) as follows:

```c
struct prometheus_histogram *prometheus_metrics_create_histogram_exponential(
    struct prometheus_metrics *metrics,
    const char                *name,
    const char                *help,
    uint64_t                   count);    // Number of buckets
```

Or with linear buckets (start, start+increment, start+2*increment, ...) as follows:

```c
struct prometheus_histogram *prometheus_metrics_create_histogram_linear(
    struct prometheus_metrics *metrics,
    const char                *name,
    const char                *help,
    uint64_t                   start,     // Starting value
    uint64_t                   increment, // Bucket size
    uint64_t                   count);    // Number of buckets
```

The name must be [A-Z-a-z0-9_]+ and the help is an english string describing the metric.

May return NULL if name contains illegal characters.

A histogram can be optionally explicitly destroyed as follows:
```c
void prometheus_histogram_destroy(
    struct prometheus_metrics *metrics,
    struct prometheus_histogram *histogram);
```

If a histogram is destroyed, all series of the histogram are implicitly destroyed as well.

If a histogram is not explicitly destroyed it will be automatically destroyed when the global context is destroyed.

One or more series of a histogram metric can be created as follows:

```c
struct prometheus_histogram_series *prometheus_histogram_create_series(
    struct prometheus_histogram *histogram,
    const char                 **label_names,
    const char                 **label_values,
    int                          num_labels);
```

The provided array of label name=value pairs become the series labels, in addition to those provided globally at initialization.

Label names must be [A-Za-z0-9_]+ and label values may be any character except double quote.

May return NULL if any label name or value contains illegal characters.

A histogram series may be optionally explicitly destroyed as follows:
```c
void prometheus_histogram_destroy_series(
    struct prometheus_histogram        *histogram,
    struct prometheus_histogram_series *series);
```

In order to add samples to a histogram, one or more handle instances must be created:

```c
struct prometheus_histogram_instance *prometheus_histogram_series_create_instance(
    struct prometheus_histogram_series *series);
```

Handle instances can be optionally explicitly destroyed as follows:
```c
void prometheus_histogram_series_destroy_instance(
    struct prometheus_histogram_series   *series,
    struct prometheus_histogram_instance *instance);
```

The histogram's buckets, sum, and count become the sum of the values of its instance handles.

The handle instance values can be manipulated as follows:

```c
void prometheus_histogram_sample(
    struct prometheus_histogram_instance *instance,
    int64_t                               value);
```

