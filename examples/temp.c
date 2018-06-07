// This example program demonstrates using libsensors to gather information
// about hardware temperature, and export them as Prometheus metrics.

#include <time.h>
#include <prometheus.h>
#include <sensors/sensors.h>

#define REFRESH_INTERVAL_SECS 3 // How often to take temperature readings

void gather_sensors(prom_metric_set *metrics)
{
  sensors_init(NULL);

  // We can use the same gauge metric definition for all temperature values
  prom_metric_def temp_gauge = {"temp", "The temperature",
    PROM_METRIC_TYPE_GAUGE};

  prom_register(metrics, &temp_gauge);

  while (1)
  {
    sensors_chip_name const * cn;
    int c = 0;
    while ((cn = sensors_get_detected_chips(NULL, &c)) != 0) {
      sensors_feature const *feat;
      int f = 0;

      while ((feat = sensors_get_features(cn, &f)) != 0) {
        sensors_subfeature const *subf;
        int s = 0;

        while ((subf = sensors_get_all_subfeatures(cn, feat, &s)) != 0) {
          double val;
          if ((subf->flags & SENSORS_MODE_R) &&
              subf->type == SENSORS_SUBFEATURE_TEMP_INPUT) {

            int rc = sensors_get_value(cn, subf->number, &val);
            if (rc == 0) {
              // Dynamically create metrics for the available features
              prom_label l_prefix = {"prefix", cn->prefix};
              prom_label l_feat = {"feature", feat->name};
              prom_metric *m = prom_get(metrics, &temp_gauge,
                  2, l_prefix, l_feat);

              // Set their values
              m->value = val;
            } else {
              printf("ERROR %d\n", rc);
            }
          }
        }
      }
    }
    // Flush will write out the latest values
    prom_flush(metrics);
    sleep(REFRESH_INTERVAL_SECS);
  }
}

int main()
{
  // Create an initialize our metric set
  prom_metric_set my_metrics;
  prom_init(&my_metrics);

  // Start the server and start gathering data
  int pid = fork();
  if (pid != 0)
    prom_start_server(&my_metrics, 5950);
  else
    gather_sensors(&my_metrics);
}
