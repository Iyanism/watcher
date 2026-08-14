# Watcher

A lightweight, real-time system monitor for Linux written in C. It renders CPU, memory, disk, GPU, network, and the top CPU-consuming processes in a live terminal dashboard.

## Features

- **CPU usage** — overall system CPU percentage
- **RAM** — total / used / free memory and usage percentage (via `MemAvailable`)
- **Disk** — total / used / free space for the current working directory's mount
- **GPU** — utilization, VRAM, and temperature (NVIDIA via `nvidia-smi`)
- **Network** — live download/upload speed in bits per second (`b/s`, `Kb/s`, `Mb/s`, `Gb/s`)
- **Processes** — top 20 processes by CPU usage with PID, user, memory, state, and command
- **System info** — current date/time and system uptime

## Requirements

- Linux with `/proc` filesystem
- A C compiler (`cc`/`gcc`)
- `make`
- For GPU metrics: an NVIDIA GPU with `nvidia-smi` on `PATH` (GPU section shows `unavailable` otherwise)

## Build

```sh
make
```

This produces the `watcher` binary. To compile with warnings:

```sh
cc -Wall -Wextra src/watcher.c src/process_monitor.c src/monitor_ui.c src/network_monitor.c -o watcher
```

## Run

```sh
./watcher
```

Press `Ctrl+C` to exit.

## Usage

```text
┌────────────────────────────────────────────────────────────────────────────┐
│                               SYSTEM MONITOR                               │
│ 2026-08-15 01:03:29    Uptime: 07:18:02                                    │
├─ CPU ──────────────────────────────────────────────────────────────────────┤
│ CPU Usage     12.9%  ██░░░░░░░░░░░░░░░░░░                                  │
├─ Memory / Disk ────────────────────────────────────────────────────────────┤
│ RAM Usage     32.3%  ██████░░░░░░░░░░░░░░                                  │
│ RAM           Used 4.96 / 15.33 GB   Free 10.37 GB                         │
│ Disk Usage    12.4%  ██░░░░░░░░░░░░░░░░░░                                  │
│ Disk          Used 57.60 / 463.17 GB   Free 405.57 GB                      │
├─ GPU ──────────────────────────────────────────────────────────────────────┤
│ GPU Usage     12.0%  ██░░░░░░░░░░░░░░░░░░                                  │
│ GPU           Temp 47 C   Used 0.04 / 6.00 GB                              │
├─ Network ──────────────────────────────────────────────────────────────────┤
│ Download      ↓ 880.6 Kb/s                                                 │
│ Upload        ↑ 82.5 Kb/s                                                  │
│ Net           Interface wlp0s20f3                                          │
├─ Processes (Top 20 by CPU) ────────────────────────────────────────────────┤
│     PID  USER           CPU%   MEM% STATE  COMMAND                         │
│    7081  pradeep        81.3    5.5     S  opencode                        │
│    25475 pradeep        10.9    2.4     S  brave                           │
│ ...                                                                        │
│ Total processes: 490                                                       │
└────────────────────────────────────────────────────────────────────────────┘
```

The dashboard refreshes roughly every second.

## Project structure

```
src/
├── watcher.c           Main loop: collects metrics, renders, sleeps
├── process_monitor.c   Process enumeration + top-N CPU sampling (/proc)
├── network_monitor.c   Interface speed from /proc/net/dev
├── monitor_ui.c        Box-drawing dashboard renderer
```

## How it works

All metrics are read from the Linux `/proc` filesystem and standard syscalls — no external libraries. Rate-based metrics (CPU, network, process CPU) use a two-snapshot delta over the render interval:

1. A warm-up read seeds the previous snapshot
2. Each tick reads fresh counters and computes the difference since the last tick
3. Results are formatted into a `SystemMetric` struct and drawn by `monitor_ui`

## License

Unspecified.