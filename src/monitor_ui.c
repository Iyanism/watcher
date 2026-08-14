#include <stdio.h>
#include <string.h>
#include <time.h>

#include "monitor_ui.h"

#define CW 76
#define INNER (CW - 2)
#define BAR_LEN 20

static int utf8_char_len(unsigned char c) {
  if (c >= 0xF0)
    return 4;
  if (c >= 0xE0)
    return 3;
  if (c >= 0xC0)
    return 2;
  return 1;
}

static void line(const char *buf) {
  int col = 0;
  const unsigned char *p = (const unsigned char *)buf;

  printf("│ ");
  while (*p && col < INNER) {
    int n = utf8_char_len(*p);
    if (col + 1 > INNER)
      break;
    fwrite(p, 1, n, stdout);
    p += n;
    col++;
  }
  while (col < INNER) {
    putchar(' ');
    col++;
  }
  printf(" │\n");
}

static void border(void) {
  fputs("\u250C", stdout);
  for (int i = 0; i < CW; i++)
    fputs("\u2500", stdout);
  fputs("\u2510\n", stdout);
}

static void section_rule(const char *label) {
  int dashes = CW - (int)strlen(label) - 3;
  fputs("\u251C\u2500 ", stdout);
  fputs(label, stdout);
  putchar(' ');
  for (int i = 0; i < dashes; i++)
    fputs("\u2500", stdout);
  fputs("\u2524\n", stdout);
}

static void centered(const char *text) {
  int len = strlen(text);
  int pad = (INNER - len) / 2;
  if (pad < 0)
    pad = 0;
  char buf[256];
  snprintf(buf, sizeof(buf), "%*s%s", pad, "", text);
  line(buf);
}

static void metric_line(const char *name, double percent) {
  char bar[BAR_LEN * 3 + 1];
  int filled = (int)(percent * BAR_LEN / 100.0);
  if (filled < 0)
    filled = 0;
  if (filled > BAR_LEN)
    filled = BAR_LEN;

  char *p = bar;
  for (int i = 0; i < BAR_LEN; i++) {
    memcpy(p, i < filled ? "\u2588" : "\u2591", 3);
    p += 3;
  }
  *p = '\0';

  char buf[256];
  snprintf(buf, sizeof(buf), "%-12s %5.1f%%  %s", name, percent, bar);
  line(buf);
}

static void value_line(const char *name, const char *detail) {
  char buf[256];
  snprintf(buf, sizeof(buf), "%-12s  %s", name, detail);
  line(buf);
}

void monitor_render(const SystemMetric *m, const ProcessList *processes) {
  border();

  centered("SYSTEM MONITOR");

  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  char timestr[32];
  strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", tm);

  char buf[CW + 1];
  snprintf(buf, sizeof(buf), "%s    Uptime: %02d:%02d:%02d", timestr,
           m->uptime_sec / 3600, (m->uptime_sec / 60) % 60, m->uptime_sec % 60);
  line(buf);

  section_rule("CPU");
  metric_line("CPU Usage", m->cpu_percent);

  section_rule("Memory / Disk");
  if (m->ram_available) {
    metric_line("RAM Usage", m->ram_percent);
    snprintf(buf, sizeof(buf), "Used %.2f / %.2f GB   Free %.2f GB",
             m->ram_used_gb, m->ram_total_gb, m->ram_free_gb);
    value_line("RAM", buf);
  } else {
    line("RAM Usage: unavailable");
  }

  if (m->disk_available) {
    metric_line("Disk Usage", m->disk_percent);
    snprintf(buf, sizeof(buf), "Used %.2f / %.2f GB   Free %.2f GB",
             m->disk_used_gb, m->disk_total_gb, m->disk_free_gb);
    value_line("Disk", buf);
  } else {
    line("Disk Usage: unavailable");
  }

  section_rule("GPU");
  if (m->gpu_available) {
    metric_line("GPU Usage", m->gpu_percent);
    snprintf(buf, sizeof(buf), "Temp %.0f C   Used %.2f / %.2f GB",
             m->gpu_temp_c, m->gpu_used_gb, m->gpu_total_gb);
    value_line("GPU", buf);
  } else {
    line("GPU Usage: unavailable");
  }

  section_rule("Processes (Top 20 by CPU)");
  if (processes) {
    double total_ram_kb = m->ram_available ? m->ram_total_gb * 1024 * 1024 : 0;

    snprintf(buf, sizeof(buf), "%7s  %-12.12s %6s %6s %5s  %s", "PID", "USER",
             "CPU%", "MEM%", "STATE", "COMMAND");
    line(buf);

    for (int i = 0; i < processes->count; i++) {
      double mem = total_ram_kb
                       ? processes->items[i].mem_kb / total_ram_kb * 100.0
                       : 0.0;
      snprintf(buf, sizeof(buf), "%7d  %-12.12s %6.1f %6.1f %5c  %.32s",
               processes->items[i].pid, processes->items[i].user,
               processes->items[i].cpu_percent, mem,
               processes->items[i].state, processes->items[i].name);
      line(buf);
    }

    snprintf(buf, sizeof(buf), "Total processes: %d", processes->total);
    line(buf);
  } else {
    line("Processes: unavailable");
  }

  border();
}