# Plant Garden Simulator

A multi-process simulation program written in C that demonstrates advanced systems programming concepts including process management, signal handling, and inter-process communication.

## Overview

This program simulates a gardener managing three plant processes. The parent process (gardener) can feed, water, and monitor three child processes (plants) that consume resources, grow, and eventually either die or are sold.

## Key Features

- **Multi-process Architecture**: Parent process spawns and manages 3 child processes
- **Signal-based Communication**: Uses SIGUSR1 (feed) and SIGUSR2 (water) for plant care
- **Real-time Resource Management**: Plants consume fertilizer and water over time
- **Multiple Terminal Display**: Each plant displays status in its own terminal window
- **Process State Tracking**: Monitors plant lifecycle (alive, sold, dead)
- **Robust Signal Handling**: SIGCHLD handler captures child exit codes

## Technical Implementation

### Process Communication
- **SIGUSR1**: Feed command (adds 10,000mg fertilizer)
- **SIGUSR2**: Water command (adds 1,000mL water)
- **SIGCHLD**: Parent captures child exit status
- **SIGTERM**: Graceful plant termination on quit

### Resource System
- **Fertilizer**: 10,000mg initial, consumes 1,000-3,000mg/second
- **Water**: 1,000mL initial, consumes 100-300mL/second  
- **Growth**: Plants grow every 5 seconds, sold after 10 growth cycles
- **Death**: Plants die after 5 seconds with resources ≤ 0

### Terminal Management
- Automatically discovers available `/dev/pts/` terminals
- Assigns separate output terminal to each plant
- Parent uses first terminal for command interface

## Compilation
```bash
gcc -Wall -g p7.c -o plant_garden
