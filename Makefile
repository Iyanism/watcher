CC = cc
CFLAGS = -Wall -Wextra

SRCS = src/watcher.c src/process_monitor.c src/monitor_ui.c src/network_monitor.c

watcher: $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o watcher

clean:
	rm -f watcher

.PHONY: clean
