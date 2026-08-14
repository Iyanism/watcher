#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include "monitor_ui.h"
#include "process_monitor.h"

double get_cpu_usage() {
  char buffer[1024];
  static unsigned long long prev_total, prev_idle;
  unsigned long long user, nice, system, idle, total, total_diff, idle_diff;

  FILE *file = fopen("/proc/stat", "r");
  if (!file)
    return 0.0;

  fgets(buffer, sizeof(buffer), file);
  fclose(file);

  sscanf(buffer, "cpu %llu %llu %llu %llu", &user, &nice, &system, &idle);

  total = user + nice + system + idle;

  if (prev_total == 0) {
    prev_total = total;
    prev_idle = idle;
    return 0.0;
  }

  total_diff = total - prev_total;
  idle_diff = idle - prev_idle;

  prev_total = total;
  prev_idle = idle;

  if (total_diff == 0)
    return 0.0;

  return ((double)(total_diff - idle_diff) / total_diff) * 100.0;
}

int get_ram_usage(double *total, double *used) {
  FILE *file = fopen("/proc/meminfo", "r");
  if (!file)
    return -1;

  char line[128];
  unsigned long long mem_total = 0, mem_available = 0;

  while (fgets(line, sizeof(line), file)) {
    if (sscanf(line, "MemTotal: %llu kB", &mem_total) == 1)
      continue;
    if (sscanf(line, "MemAvailable: %llu kB", &mem_available) == 1)
      continue;
  }
  fclose(file);

  if (mem_total == 0 || mem_available == 0)
    return -1;

  *total = (double)mem_total / (1024 * 1024);
  *used = (double)(mem_total - mem_available) / (1024 * 1024);

  return 0;
}

int get_gpu_usage(double *usage, double *used_gb, double *total_gb,
                  double *temp) {
  FILE *file =
      popen("nvidia-smi --query-gpu=utilization.gpu,memory.used,memory.total,"
            "temperature.gpu --format=csv,noheader",
            "r");
  if (!file)
    return -1;

  char line[256];
  int mem_used, mem_total;
  if (!fgets(line, sizeof(line), file)) {
    pclose(file);
    return -1;
  }
  pclose(file);

  if (sscanf(line, "%lf %%, %d MiB, %d MiB, %lf", usage, &mem_used, &mem_total,
             temp) != 4)
    return -1;

  *used_gb = (double)mem_used / 1024.0;
  *total_gb = (double)mem_total / 1024.0;

  return 0;
}

int get_disk_usage(const char *path, double *total, double *used) {
  struct statvfs stat;

  if (statvfs(path, &stat) != 0)
    return -1;

  *total = (double)stat.f_blocks * stat.f_frsize / (1024 * 1024 * 1024);
  *used = (double)(stat.f_blocks - stat.f_bavail) * stat.f_frsize /
          (1024 * 1024 * 1024);

  return 0;
}

void clear_screen() { printf("\033[2J\033[H"); }

int main() {
  SystemMetric metric;
  memset(&metric, 0, sizeof(metric));

  get_cpu_usage();
  process_monitor_init();
  sleep(1);

  while (1) {
    metric.cpu_percent = get_cpu_usage();

    metric.ram_available =
        get_ram_usage(&metric.ram_total_gb, &metric.ram_used_gb) == 0;
    if (metric.ram_available) {
      metric.ram_free_gb = metric.ram_total_gb - metric.ram_used_gb;
      metric.ram_percent = metric.ram_used_gb / metric.ram_total_gb * 100.0;
    }

    metric.disk_available =
        get_disk_usage(".", &metric.disk_total_gb, &metric.disk_used_gb) == 0;
    if (metric.disk_available) {
      metric.disk_free_gb = metric.disk_total_gb - metric.disk_used_gb;
      metric.disk_percent = metric.disk_used_gb / metric.disk_total_gb * 100.0;
    }

    metric.gpu_available =
        get_gpu_usage(&metric.gpu_percent, &metric.gpu_used_gb,
                      &metric.gpu_total_gb, &metric.gpu_temp_c) == 0;

    struct sysinfo info;
    if (sysinfo(&info) == 0)
      metric.uptime_sec = info.uptime;

    ProcessList processes;
    process_monitor_collect(&processes, 20);

    clear_screen();
    monitor_render(&metric, &processes);
    process_list_free(&processes);

    sleep(1);
  }
  return 0;
}