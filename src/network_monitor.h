#ifndef NETWORK_MONITOR_H
#define NETWORK_MONITOR_H

#include <stddef.h>

typedef struct {
  char iface[16];
  double rx_bps;
  double tx_bps;
} NetworkSpeed;

int network_monitor_init(void);
int network_monitor_collect(NetworkSpeed *net);
void format_speed(double bits_per_sec, char *buf, size_t size);

#endif
