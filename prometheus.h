#ifndef PROMETHEUS_H
#define PROMETHEUS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

static const char PROM_METRIC_TYPE_COUNTER[]   = "counter";
static const char PROM_METRIC_TYPE_GAUGE[]     = "gauge";
static const char PROM_METRIC_TYPE_HISTOGRAM[] = "histogram";
static const char PROM_METRIC_TYPE_SUMMARY[]   = "summary";

#ifndef PROM_MAX_LABELS
#define PROM_MAX_LABELS 50
#endif

#ifndef PROM_MAX_METRICS
#define PROM_MAX_METRICS 256
#endif

#define PROM_CONN_BACKLOG 10

// Generic definition for a metric including name, help and type
typedef struct prom_metric_def
{
	char *name;
	char *help;
	const char *type;
} prom_metric_def;

// Key-value pair representing a label name with an assigned value
typedef struct prom_label
{
	char *key;
	char *value;
} prom_label;

// Represents an instance of a metric with a given value and set of labels
typedef struct prom_metric
{
	int num_labels;
	struct prom_label labels[PROM_MAX_LABELS];
	double value;
} prom_metric;

typedef struct prom_metric_def_set
{
  prom_metric_def *def;
  int n_metrics;
  prom_metric *metrics[PROM_MAX_METRICS];
} prom_metric_def_set;

// Container for a set of references to prom_metrics
typedef struct prom_metric_set
{
  int n_defs;
  prom_metric_def_set *defs[PROM_MAX_METRICS];
} prom_metric_set;

void prom_init(prom_metric_set *s)
{
  s->n_defs = 0;
}

void prom_register(prom_metric_set *s, prom_metric_def *d)
{
  // Check if we already have this definition
  int existing = -1;
  for (int i = 0; i < s->n_defs; i++)
  {
    if (s->defs[i]->def == d)
    {
      existing = i;
    }
  }
  if (existing == -1)
  {
    // It doesn't exist, create it
    existing = s->n_defs;
    s->n_defs++;
    s->defs[existing] = malloc(sizeof(prom_metric_def_set));
    s->defs[existing]->def = d;
  }
}

// Initializes a prom_metric with a zero value and empty label set
void prom_metric_init(prom_metric *m)
{
	m->num_labels = 0;
	memset(&m->labels, 0, sizeof(prom_label) * PROM_MAX_LABELS);
	m->value = 0;
}

// Sets a label key and value on the given metric value
void prom_metric_set_label(prom_metric *m, char *key, char *value)
{
	m->labels[m->num_labels].key = key;
	m->labels[m->num_labels].value = value;
	m->num_labels++;
}

prom_metric *prom_get_or_create(prom_metric_set *s, prom_metric_def *d, int n, ...)
{
  va_list args;
  va_start(args, n);
  prom_label ulabels[PROM_MAX_LABELS];
  for (int i = 0; i < n; i++)
  {
    ulabels[i] = va_arg(args, prom_label);
  }
  va_end(args);

  int found = 0;
  prom_metric *m_found;
  prom_metric_def_set *ds;

  for (int i = 0; i < s->n_defs; i++)
  {
    if (s->defs[i]->def == d)
    {
      ds = s->defs[i];
      for (int j = 0; j < ds->n_metrics; j++)
      {
        // Compare labels
        int labels_match = 1;
        prom_metric *m = ds->metrics[j];
        if (m->num_labels != n)
          continue;
        for (int l = 0; l < m->num_labels; l++)
        {
          prom_label mlab = m->labels[l];
          prom_label ulab = ulabels[l];
          if (strcmp(mlab.key, ulab.key) || strcmp(mlab.value, ulab.value))
            labels_match = 0;
        }
        if (labels_match == 1)
        {
          found = 1;
          break;
        }
      }
      break;
    }
  }

  // Create if not found
  if (found == 0)
  {
    ds->metrics[ds->n_metrics] = malloc(sizeof(prom_metric));
    ds->n_metrics++;
    for (int i = 0; i < n; i++)
    {
      prom_metric_set_label(ds->metrics[ds->n_metrics-1], ulabels[i].key, ulabels[i].value);
    }
    return ds->metrics[ds->n_metrics-1];
  }
}

// Prints the metric value to the given IO
void prom_metric_write(prom_metric_def_set *s, int f)
{
  char buf[1024];
  // Write the header comments
  sprintf(buf, "# TYPE %s %s\n", s->def->name, s->def->type);
  write(f, buf, strlen(buf));
  sprintf(buf, "# HELP %s %s\n", s->def->name, s->def->help);
  write(f, buf, strlen(buf));

  // Write the metric values
  for (int i = 0; i < s->n_metrics; i++)
  {
    prom_metric *m = s->metrics[i];
    write(f, s->def->name, strlen(s->def->name));
    if (m->num_labels > 0)
    {
      write(f, "{", 1);
      for (int i = 0; i < m->num_labels; i++)
      {
        sprintf(buf, "%s=\"%s\"", m->labels[i].key, m->labels[i].value);
        write(f, buf, strlen(buf));
        if (i < (m->num_labels - 1))
        {
          write(f, ",", 1);
        }
      }
      write(f, "}", 1);
    }
    sprintf(buf, " %f\n", m->value);
    write(f, buf, strlen(buf));
  }
}

void prom_http_write_header(int sock)
{
  char *resp = "HTTP/1.1 200 OK\n";
  write(sock, resp, strlen(resp));
}

int prom_start_server(prom_metric_set *s)
{
  int sockfd;
  int yes=1;
  int gai_ret;
  struct addrinfo hints, *servinfo, *p;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  gai_ret = getaddrinfo(NULL, "5950", &hints, &servinfo);
  if (gai_ret != 0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_ret));
    return -1;
  }

  // Once we have a list of potential interfaces, loop through them
  // and try to set up a socket on each. Quit looping the first time
  // we have success.
  for(p = servinfo; p != NULL; p = p->ai_next) {

    // Try to make a socket based on this candidate interface
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
        p->ai_protocol)) == -1) {
      //perror("server: socket");
      continue;
    }

    // SO_REUSEADDR prevents the "address already in use" errors
    // that commonly come up when testing servers.
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
        sizeof(int)) == -1) {
      perror("setsockopt");
      close(sockfd);
      freeaddrinfo(servinfo); // all done with this structure
      return -2;
    }

    // See if we can bind this socket to this local IP address. This
    // associates the file descriptor (the socket descriptor) that
    // we will read and write on with a specific IP address.
    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      //perror("server: bind");
      continue;
    }

    // If we got here, we got a bound socket and we're done
    break;
  }

  freeaddrinfo(servinfo); // all done with this structure

  // If p is NULL, it means we didn't break out of the loop, above,
  // and we don't have a good socket.
  if (p == NULL)  {
    fprintf(stderr, "webserver: failed to find local address\n");
    return -3;
  }

  // Start listening. This is what allows remote computers to connect
  // to this socket/IP.
  if (listen(sockfd, PROM_CONN_BACKLOG) == -1) {
    //perror("listen");
    close(sockfd);
    return -4;
  }

  int newfd;  // listen on sock_fd, new connection on newfd
  struct sockaddr_storage their_addr; // connector's address information

  // Start serving
  while (1)
  {
    socklen_t sin_size = sizeof their_addr;
    newfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

    int status;
    char readBuffer[1024];
    do {
      status = recv(newfd, readBuffer, sizeof readBuffer, MSG_DONTWAIT);
    } while (status > 0);

    prom_http_write_header(newfd);
    // Separator for HTTP body
    write(newfd, "\n", 1);
    for (int i = 0; i < s->n_defs; i++)
    {
      prom_metric_write(s->defs[i], newfd);
    }
    shutdown(newfd, 1);
    close(newfd);
  }
}

#endif // PROMETHEUS_H
