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
  prom_metric_def current_time = {"current_time",
      "The time that it is right now", PROM_METRIC_TYPE_COUNTER};

  // Register your metric definition to the set
  prom_register(&metrics, &current_time);

  // You can obtain a metric instance by passing any number of labels
  prom_label foo_label = {"foo", "bar"};
  prom_metric *cur_time_metric = prom_get(&metrics, &current_time, 1,
      foo_label);

  // Create a fork process to run the HTTP server
  int pid = fork();
  if (pid == 0) {
    while (1) {
      // In the main thread, we can update the metric values periodically
      cur_time_metric->value = time(NULL);
      // prom_flush must be called to update the current metric values
      prom_flush(&metrics);
      sleep(3);
    }
  } else {
    prom_start_server(&metrics, 5950);
  }
}
