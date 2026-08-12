#include <stdio.h>
#include <sys/sysinfo.h>
#include <unistd.h>

void clear_screen() { printf("\033[2J\033[H"); }

double get_cpu_usage() {
  char buffer[1024];
  static unsigned long long prev_total, prev_idle;
  unsigned long long user, nice, system, idle, total, total_diff, idle_diff;

  double cpu_usage = 0.0;

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

int main() {
  clear_screen();
  printf("System Monitor - Press Ctrl+C to exit\n");
  printf("====================================\n\n");
  for (int i = 0; i < 10; i++) {
    double cpu_usage = get_cpu_usage();
    printf("CPU Usage: %.1f%%", cpu_usage);
    printf("----------------------------------------------------------\n");
    sleep(1);
  }
  return 0;
}
