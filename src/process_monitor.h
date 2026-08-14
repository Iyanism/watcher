#ifndef PROCESS_MONITOR_H
#define PROCESS_MONITOR_H

typedef struct {
  int pid;
  char name[64];
  char user[32];
  char state;
  double cpu_percent;
  unsigned long mem_kb;
} ProcessInfo;

typedef struct {
  ProcessInfo *items;
  int count;
  int total;
} ProcessList;

int process_monitor_init(void);
int process_monitor_collect(ProcessList *list, int top_n);
void process_list_free(ProcessList *list);

#endif
