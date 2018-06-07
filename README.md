prometheus.h
============

A lightweight and simple client library for exposing metrics to
Prometheus from C applications, contained in a single header file. It handles
formatting the metrics and exposing an HTTP endpoint for the Prometheus scraper
to pick them up.

To write an exporter, start by creating a `prom_metric_set` and initializing
it.

```c
prom_metric_set metrics;
prom_init(&metrics);
```

Register metrics by creating instances of `prom_metric_def`.

```c
prom_metric_def current_time = {"current_time",
    "The current system time", PROM_METRIC_TYPE_COUNTER};
prom_register(&metrics, &current_time);
```

Obtain an instance of `prom_metric` by passing any number of labels to the
`prom_get` function. If a metric with the given combination of labels does not
exist, it will be created. Multiple calls to `prom_get` with the same labels
will return the same pointer, but the labels must always be passed in the same
order.

```c
prom_label foo = {"foo", "bar"};
prom_label bing = {"bing", "bong"};

prom_metric m = prom_get(&metrics, &current_time, 2, foo, bing);
```

You can start the server in a forked process. Then, you can manipulate the
metric values in the main thread and write them out using `prom_flush`.

```c
int pid = fork();
if (pid) {
  prom_start_server(&metrics, 50950);
} else {
  while (1) {
    m->value = time(NULL);
    prom_flush(&metrics);
    sleep(5);
  }
}
```

When you are done, call `prom_cleanup` to free the memory used by the metric
set.

```c
prom_cleanup(&metrics);
```

View the example program in this repository for more examples. You can build
it with the following command:

```bash
gcc -o stat -I. example/stat.c
```

TODO:
* Handle complex types like summary and histogram
