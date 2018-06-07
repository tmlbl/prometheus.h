#include <time.h>

#include "prometheus.h"

// A metric set contains the set of metrics for your application
prom_metric_set metrics;

// Set up an exit handler to make sure we clean up
void cleanup()
{
  prom_cleanup(&metrics);
}

int main()
{
  prom_init(&metrics);

  // Metric definitions contain the name, help, and type
  prom_metric_def example_gauge = {"example_gauge", 
      "An example of a gauge metric", PROM_METRIC_TYPE_GAUGE};

  // Register your metric definition to the set
  prom_register(&metrics, &example_gauge);

  // You can obtain a metric instance by passing any number of labels
  prom_label device_label = {"device", "/dev/sda"};
  prom_metric *device_gauge_metric = prom_get(&metrics, &example_gauge, 1,
      device_label);

  // Create a fork process to run the HTTP server
  int pid = fork();
  if (pid == 0) {
    while (1) {
      // In the main thread, we can update the metric values periodically
      device_gauge_metric->value = time(NULL);
      // prom_flush must be called to update the current metric values
      prom_flush(&metrics);
      sleep(3);
    }
  } else {
    prom_start_server(&metrics);
  }
}

