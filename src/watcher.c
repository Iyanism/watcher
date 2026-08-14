#include <stdio.h>
#include <sys/statvfs.h>
#include <unistd.h>

void clear_screen() { printf("\033[2J\033[H"); }

void print_bar(double percent, int width) {
  int filled = (int)(percent * width / 100.0);
  for (int i = 0; i < width; i++)
    putchar(i < filled ? '#' : '-');
}

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

int main() {
  double total, used, disk_total, disk_used;
  double gpu, gpu_used, gpu_total, gpu_temp;

  get_cpu_usage();
  sleep(1);

  while (1) {
    double cpu = get_cpu_usage();
    int ram_ok = get_ram_usage(&total, &used);
    int disk_ok = get_disk_usage(".", &disk_total, &disk_used);
    int gpu_ok = get_gpu_usage(&gpu, &gpu_used, &gpu_total, &gpu_temp);

    clear_screen();
    printf("System Monitor - Press Ctrl+C to exit\n");
    printf("====================================\n\n");

    printf("CPU Usage: %5.1f%% [", cpu);
    print_bar(cpu, 20);
    printf("]\n");

    if (ram_ok == 0) {
      double ram = used / total * 100.0;
      printf("RAM Usage: %5.1f%% [", ram);
      print_bar(ram, 20);
      printf("]\n");
      printf("RAM Total: %.2f GB | Used: %.2f GB | Free: %.2f GB\n", total,
             used, total - used);
    } else {
      printf("RAM Usage: unavailable\n");
    }

    if (disk_ok == 0) {
      double disk = disk_used / disk_total * 100.0;
      double disk_free = disk_total - disk_used;
      printf("\nDisk Usage: %5.1f%% [", disk);
      print_bar(disk, 20);
      printf("]\n");
      printf("Disk Total: %.2f GB | Used: %.2f GB | Free: %.2f GB\n",
             disk_total, disk_used, disk_free);
    } else {
      printf("\nDisk Usage: unavailable\n");
    }

    if (gpu_ok == 0) {
      printf("\nGPU Usage: %5.1f%% [", gpu);
      print_bar(gpu, 20);
      printf("]  Temp: %.0f C\n", gpu_temp);
      printf("GPU Total: %.2f GB | Used: %.2f GB | Free: %.2f GB\n", gpu_total,
             gpu_used, gpu_total - gpu_used);
    } else {
      printf("\nGPU Usage: unavailable\n");
    }

    printf("\n");
    sleep(1);
  }
  return 0;
}