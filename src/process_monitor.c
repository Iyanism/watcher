#include <ctype.h>
#include <dirent.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "process_monitor.h"

typedef struct {
  pid_t pid;
  unsigned long long utime;
  unsigned long long stime;
} CpuSnapshot;

typedef struct {
  pid_t pid;
  char name[64];
  char user[32];
  char state;
  double cpu_percent;
  unsigned long mem_kb;
  unsigned long long utime;
  unsigned long long stime;
} ProcEntry;

static CpuSnapshot *prev_snap = NULL;
static int prev_count = 0;
static long clock_ticks = 0;
static struct timeval prev_time;

static int read_process_stat(pid_t pid, char *state, unsigned long long *utime,
                             unsigned long long *stime) {
  char path[256];
  char buffer[1024];
  FILE *file;

  snprintf(path, sizeof(path), "/proc/%d/stat", pid);
  file = fopen(path, "r");
  if (!file)
    return 0;

  if (!fgets(buffer, sizeof(buffer), file)) {
    fclose(file);
    return 0;
  }
  fclose(file);

  char *tail = strrchr(buffer, ')');
  if (!tail)
    return 0;

  return sscanf(tail + 1,
                " %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %llu %llu", state,
                utime, stime) == 3;
}

static void read_process_name(pid_t pid, char *name, size_t size) {
  char path[256];
  FILE *file;

  snprintf(path, sizeof(path), "/proc/%d/comm", pid);
  file = fopen(path, "r");
  if (!file) {
    snprintf(name, size, "?");
    return;
  }

  if (!fgets(name, size, file))
    snprintf(name, size, "?");
  fclose(file);

  char *nl = strchr(name, '\n');
  if (nl)
    *nl = '\0';
}

static void read_process_user(pid_t pid, char *user, size_t size) {
  char path[256];
  char line[256];
  FILE *file;
  int uid = -1;

  snprintf(path, sizeof(path), "/proc/%d/status", pid);
  file = fopen(path, "r");
  if (!file) {
    snprintf(user, size, "unknown");
    return;
  }

  while (fgets(line, sizeof(line), file)) {
    if (sscanf(line, "Uid: %d", &uid) == 1)
      break;
  }
  fclose(file);

  struct passwd *pw = uid >= 0 ? getpwuid((uid_t)uid) : NULL;
  snprintf(user, size, "%s", pw ? pw->pw_name : "unknown");
}

static void read_process_mem(pid_t pid, unsigned long *mem_kb) {
  char path[256];
  char line[256];
  FILE *file;
  unsigned long resident_pages = 0;
  static long page_size = 0;

  if (page_size == 0)
    page_size = sysconf(_SC_PAGESIZE);

  snprintf(path, sizeof(path), "/proc/%d/statm", pid);
  file = fopen(path, "r");
  if (!file)
    return;

  if (fgets(line, sizeof(line), file))
    sscanf(line, "%*u %lu", &resident_pages);
  fclose(file);

  *mem_kb = resident_pages * page_size / 1024;
}

static ProcEntry *read_all_processes(int *out_count) {
  DIR *dir = opendir("/proc");
  if (!dir)
    return NULL;

  ProcEntry *entries = NULL;
  int count = 0, cap = 0;

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (!isdigit((unsigned char)entry->d_name[0]))
      continue;

    pid_t pid = atoi(entry->d_name);
    char state;
    unsigned long long utime = 0, stime = 0;

    if (!read_process_stat(pid, &state, &utime, &stime))
      continue;

    if (count == cap) {
      cap = cap ? cap * 2 : 256;
      ProcEntry *tmp = realloc(entries, cap * sizeof(ProcEntry));
      if (!tmp) {
        free(entries);
        closedir(dir);
        return NULL;
      }
      entries = tmp;
    }

    entries[count].pid = pid;
    entries[count].state = state;
    entries[count].utime = utime;
    entries[count].stime = stime;
    entries[count].cpu_percent = 0.0;
    entries[count].mem_kb = 0;
    read_process_name(pid, entries[count].name, sizeof(entries[count].name));
    read_process_user(pid, entries[count].user, sizeof(entries[count].user));
    read_process_mem(pid, &entries[count].mem_kb);
    count++;
  }

  closedir(dir);
  *out_count = count;
  return entries;
}

static unsigned long long find_prev_cpu(pid_t pid) {
  for (int i = 0; i < prev_count; i++) {
    if (prev_snap[i].pid == pid)
      return prev_snap[i].utime + prev_snap[i].stime;
  }
  return 0;
}

static int cmp_cpu_desc(const void *a, const void *b) {
  const ProcEntry *pa = a, *pb = b;
  return (pa->cpu_percent > pb->cpu_percent) ? -1
         : (pa->cpu_percent < pb->cpu_percent) ? 1
                                               : 0;
}

int process_monitor_init(void) {
  int count = 0;
  ProcEntry *cur = read_all_processes(&count);
  if (!cur)
    return -1;

  if (clock_ticks == 0)
    clock_ticks = sysconf(_SC_CLK_TCK);
  gettimeofday(&prev_time, NULL);

  prev_snap = malloc(count * sizeof(CpuSnapshot));
  if (count && !prev_snap) {
    free(cur);
    return -1;
  }

  for (int i = 0; i < count; i++) {
    prev_snap[i].pid = cur[i].pid;
    prev_snap[i].utime = cur[i].utime;
    prev_snap[i].stime = cur[i].stime;
  }
  prev_count = count;

  free(cur);
  return 0;
}

int process_monitor_collect(ProcessList *list, int top_n) {
  list->items = NULL;
  list->count = 0;
  list->total = 0;

  if (clock_ticks == 0)
    clock_ticks = sysconf(_SC_CLK_TCK);

  int count = 0;
  ProcEntry *cur = read_all_processes(&count);
  if (!cur)
    return -1;

  struct timeval now;
  gettimeofday(&now, NULL);
  double elapsed = (now.tv_sec - prev_time.tv_sec) +
                   (now.tv_usec - prev_time.tv_usec) / 1000000.0;

  for (int i = 0; i < count; i++) {
    double cpu = 0.0;
    if (elapsed > 0) {
      unsigned long long prev_cpu = find_prev_cpu(cur[i].pid);
      if (prev_cpu > 0) {
        unsigned long long diff = cur[i].utime + cur[i].stime - prev_cpu;
        cpu = (double)diff / clock_ticks / elapsed * 100.0;
      }
    }
    cur[i].cpu_percent = cpu;
  }

  qsort(cur, count, sizeof(ProcEntry), cmp_cpu_desc);

  int n = count < top_n ? count : top_n;
  list->items = malloc((n ? n : 1) * sizeof(ProcessInfo));
  if (!list->items) {
    free(cur);
    return -1;
  }

  for (int i = 0; i < n; i++) {
    list->items[i].pid = cur[i].pid;
    list->items[i].state = cur[i].state;
    list->items[i].cpu_percent = cur[i].cpu_percent;
    list->items[i].mem_kb = cur[i].mem_kb;
    snprintf(list->items[i].name, sizeof(list->items[i].name), "%s",
             cur[i].name);
    snprintf(list->items[i].user, sizeof(list->items[i].user), "%s",
             cur[i].user);
  }
  list->count = n;
  list->total = count;

  free(prev_snap);
  prev_snap = malloc(count * sizeof(CpuSnapshot));
  if (count && !prev_snap) {
    free(cur);
    return -1;
  }

  for (int i = 0; i < count; i++) {
    prev_snap[i].pid = cur[i].pid;
    prev_snap[i].utime = cur[i].utime;
    prev_snap[i].stime = cur[i].stime;
  }
  prev_count = count;
  prev_time = now;

  free(cur);
  return 0;
}

void process_list_free(ProcessList *list) {
  free(list->items);
  list->items = NULL;
  list->count = 0;
  list->total = 0;
}
