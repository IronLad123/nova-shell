# NovaShell

NovaShell is a custom Unix-like shell written in C to demonstrate core operating system concepts such as process creation, inter-process communication, safety mechanisms, and resource monitoring.

## Features
- Command execution using fork and execvp
- Pipes and output redirection
- Background process support
- Safety layer for dangerous commands
- Command suggestion on typos
- Command history
- Visual CPU and memory monitoring (signature feature)
- Session summary

## Build Instructions
```bash
make
./build/nova-shell
