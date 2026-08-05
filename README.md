# 🐚 NovaShell — High-Performance Custom Unix Shell

<p align="center">
  <img src="https://img.shields.io/badge/Language-C99-00599C?style=for-the-badge&logo=c" alt="Language C" />
  <img src="https://img.shields.io/badge/OS-Unix%20%7C%20Linux%20%7C%20macOS-000000?style=for-the-badge&logo=apple" alt="OS" />
  <img src="https://img.shields.io/badge/Build-Makefile-10b981?style=for-the-badge&logo=gnu" alt="Build Makefile" />
  <img src="https://img.shields.io/badge/Status-Production%20Ready-34d399?style=for-the-badge" alt="Status" />
</p>

<p align="center">
  <b>NovaShell is a feature-rich, high-performance Unix-like command shell implemented in C. Demonstrating low-level operating system fundamentals including process management (`fork`, `execvp`), inter-process communication (pipes), IO redirection, safety guards, and real-time CPU/memory resource monitoring.</b>
</p>

---

## 🌟 Key Features

- ⚡ **Process Lifecycle Management**: Fast process spawning with `fork()`, `execvp()`, and non-blocking background job handling (`&`).
- 📁 **Builtin Shell Navigation**: Full `cd` support (including `~` home expansion and `cd -` directory toggle), `pwd`, and `help`.
- 🔀 **Inter-Process Communication**: Pipeline execution (`cmd1 | cmd2`) connecting stdout to stdin via Unix pipes.
- 📤📥 **File IO Redirection**: Supports stdout truncation (`>`), stdout append (`>>`), and stdin input redirection (`<`).
- 🛡️ **Safety Lockdown Layer**: Intercepts and blocks destructive root commands (`rm -rf`, `shutdown`, `reboot`).
- 💡 **Typo Suggestion Engine**: Intelligent command recommendation for mistyped utility names.
- 📊 **Real-Time Resource Monitoring**: Live visual CPU/memory usage bars, wall-clock timing, and performance badges (`FAST`, `NORMAL`, `SLOW`).
- 📜 **Session History & Analytics**: Builtin command history tracking and exit session performance summary.

---

## 🏗️ Architecture & Component Overview

```text
  NovaShell Core Architecture
  ├── src/main.c      - Command loop, builtin dispatcher, env expansion, IO redirection
  ├── src/pipe.c      - Inter-process pipe handler (`dup2`, `pipe`)
  ├── src/monitor.c   - Real-time CPU & memory consumption tracker (`getrusage`, `gettimeofday`)
  ├── src/safety.c    - Danger interception layer (`rm -rf` guard)
  ├── src/history.c   - Circular command history buffer
  ├── src/suggest.c   - Levenshtein-based command suggestion engine
  └── src/summary.c   - Session statistics & command counters
```

---

## 🛠️ Build & Installation Guide

### Prerequisites
- **GCC / Clang** compiler supporting C99 standard
- **GNU Make**

### Compilation

```bash
# Clone or navigate to directory
cd nova-shell

# Compile project using Makefile
make

# Execute NovaShell
./build/nova-shell
```

### Clean Build Artifacts

```bash
make clean
```

---

## 💻 Interactive Commands & Usage Syntax

| Feature | Example Command | Description |
|---|---|---|
| **Directory Navigation** | `cd ~/Projects` | Change directory (`cd -` to return to previous path). |
| **Print Directory** | `pwd` | Print current working directory. |
| **Pipeline Execution** | `ls -la \| grep txt` | Pipe output from command 1 into command 2. |
| **Output Redirection** | `echo "Hello" > out.txt` | Truncate and write to file. |
| **Append Redirection** | `echo "Line 2" >> out.txt` | Append text to file. |
| **Input Redirection** | `cat < out.txt` | Read stdin from input file. |
| **Background Execution**| `sleep 10 &` | Execute task asynchronously in background. |
| **Command History** | `history` | List recent command execution history. |
| **Help Manual** | `help` | Display builtin command manual. |
| **Exit Shell** | `exit` | Output session telemetry summary and terminate shell. |

---

## 🧪 Testing Verification

```bash
# Test 1: Navigation & Env Expansion
nova-shell [~/Projects/nova-shell]> cd ~
nova-shell [~]> pwd
/Users/username

# Test 2: Pipe & Redirection
nova-shell [~/Projects/nova-shell]> echo "NovaShell Enterprise" > test.txt
nova-shell [~/Projects/nova-shell]> cat test.txt | grep Nova
NovaShell Enterprise

# Test 3: Safety Layer
nova-shell [~/Projects/nova-shell]> rm -rf /
⚠️ Safety Alert: Dangerous command blocked for system security.
```

---

## 👨‍💻 Developer & Attribution

Developed by **Om Srivastava ([@IronLad123](https://github.com/IronLad123))**  
*B.Tech in Computer Science & Engineering (Specialization in Data Science)*
