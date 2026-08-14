#ifndef MONITOR_UI_H
#define MONITOR_UI_H

#include "process_monitor.h"

typedef struct {
  double cpu_percent;
  int ram_available;
  double ram_total_gb, ram_used_gb, ram_free_gb, ram_percent;
  int disk_available;
  double disk_total_gb, disk_used_gb, disk_free_gb, disk_percent;
  int gpu_available;
  double gpu_percent, gpu_used_gb, gpu_total_gb, gpu_temp_c;
  int uptime_sec;
} SystemMetric;

void monitor_render(const SystemMetric *metric, const ProcessList *processes);

#endif