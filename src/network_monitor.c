#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "network_monitor.h"

static char chosen_iface[16] = "";
static unsigned long long prev_rx = 0;
static unsigned long long prev_tx = 0;
static struct timeval prev_time;
static int primed = 0;

static int read_counters(const char *iface, unsigned long long *rx,
                         unsigned long long *tx) {
  FILE *file = fopen("/proc/net/dev", "r");
  if (!file)
    return 0;

  char line[256];
  int found = 0;

  while (fgets(line, sizeof(line), file)) {
    char name[16];
    unsigned long long cur_rx, cur_tx;

    if (sscanf(line, " %15[^:]: %llu %*u %*u %*u %*u %*u %*u %*u %llu", name,
               &cur_rx, &cur_tx) != 3)
      continue;

    if (strcmp(name, iface) == 0) {
      *rx = cur_rx;
      *tx = cur_tx;
      found = 1;
      break;
    }
  }
  fclose(file);
  return found;
}

static int pick_busiest(char *out, size_t size) {
  FILE *file = fopen("/proc/net/dev", "r");
  if (!file)
    return 0;

  char line[256];
  char best[16] = "";
  unsigned long long best_total = 0;

  while (fgets(line, sizeof(line), file)) {
    char name[16];
    unsigned long long rx, tx;

    if (sscanf(line, " %15[^:]: %llu %*u %*u %*u %*u %*u %*u %*u %llu", name,
               &rx, &tx) != 3)
      continue;

    if (strcmp(name, "lo") == 0)
      continue;

    if (rx + tx > best_total) {
      best_total = rx + tx;
      snprintf(best, sizeof(best), "%s", name);
    }
  }
  fclose(file);

  if (!best[0])
    return 0;

  snprintf(out, size, "%s", best);
  return 1;
}

int network_monitor_init(void) {
  if (!pick_busiest(chosen_iface, sizeof(chosen_iface)))
    return -1;

  if (!read_counters(chosen_iface, &prev_rx, &prev_tx))
    return -1;

  gettimeofday(&prev_time, NULL);
  primed = 1;
  return 0;
}

int network_monitor_collect(NetworkSpeed *net) {
  if (!primed)
    return -1;

  unsigned long long rx = 0, tx = 0;
  if (!read_counters(chosen_iface, &rx, &tx))
    return -1;

  struct timeval now;
  gettimeofday(&now, NULL);
  double elapsed = (now.tv_sec - prev_time.tv_sec) +
                   (now.tv_usec - prev_time.tv_usec) / 1000000.0;
  if (elapsed <= 0)
    elapsed = 0.001;

  snprintf(net->iface, sizeof(net->iface), "%s", chosen_iface);
  net->rx_bps = (double)(rx - prev_rx) / elapsed * 8.0;
  net->tx_bps = (double)(tx - prev_tx) / elapsed * 8.0;

  prev_rx = rx;
  prev_tx = tx;
  prev_time = now;
  return 0;
}

void format_speed(double bits_per_sec, char *buf, size_t size) {
  if (bits_per_sec < 0)
    bits_per_sec = 0;

  if (bits_per_sec < 1000) {
    snprintf(buf, size, "%.0f b/s", bits_per_sec);
  } else if (bits_per_sec < 1000000) {
    snprintf(buf, size, "%.1f Kb/s", bits_per_sec / 1000);
  } else if (bits_per_sec < 1000000000) {
    snprintf(buf, size, "%.2f Mb/s", bits_per_sec / 1000000);
  } else {
    snprintf(buf, size, "%.2f Gb/s", bits_per_sec / 1000000000);
  }
}