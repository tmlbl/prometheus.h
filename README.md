prometheus.h
============

A lightweight and simple client library for exposing metrics to
Prometheus from C applications. It handles formatting the metrics and
exposing an HTTP endpoint for the Prometheus scraper to pick them up.

This library is a work in progress. See the example program for
documentation. You can build it with:

```bash
gcc -o stat -I. example/stat.c
```

TODO:
* Handle complex types like summary and histogram
