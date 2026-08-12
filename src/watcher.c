#include <stdio.h>
#include <sys/sysinfo.h>
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
  struct sysinfo info;

  if (sysinfo(&info) != 0)
    return -1;

  *total = (double)info.totalram * info.mem_unit / (1024 * 1024 * 1024);
  *used = (double)(info.totalram - info.freeram) * info.mem_unit /
          (1024 * 1024 * 1024);

  return 0;
}

int main() {
  double total, used;

  get_cpu_usage();
  sleep(1);

  while (1) {
    double cpu = get_cpu_usage();
    int ram_ok = get_ram_usage(&total, &used);

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
      printf("RAM Total: %.2f GB | Used: %.2f GB | Free: %.2f GB\n\n", total,
             used, total - used);
    } else {
      printf("RAM Usage: unavailable\n\n");
    }

    sleep(1);
  }
  return 0;
}
