# BugMe

A Linux debugger built from scratch in C and C++, using `ptrace`. Supports setting breakpoints, inspecting registers, and stepping through execution of any ELF binary.

Built as a learning project to understand how debuggers like gdb and lldb actually work under the hood.

## How it works

The core mechanism is the Linux `ptrace` syscall, which lets one process (the debugger) observe and control another (the tracee). A breakpoint is just one byte — `0xcc` (`int 3`) — written into the tracee's code at the target address. When the CPU executes it, a `SIGTRAP` is raised, the process pauses, and the debugger wakes up. It then restores the original byte, rewinds the instruction pointer by one, and hands control back to the user.

Register reads and writes go through `PTRACE_GETREGS` / `PTRACE_SETREGS`, which snapshot the full CPU register state of the tracee at any point.

## Architecture

The project is written in both C and C++:

- **C** (`ptraceWrappers.c`) — thin wrappers around raw `ptrace` syscalls, `fork`, `execvp`, `waitpid`
- **C++** (`Debugger`, `BreakPoint`, `Registers` classes) — owns all state and logic, calls into the C layer

```
main.cpp
  └── Debugger          owns everything, runs the command loop
        ├── Registers   fetches/writes CPU registers via ptrace
        └── BreakPoint  plants/restores int3 at a given address
```

## Building

```bash
mkdir build && cd build
cmake ..
make
```

Requires CMake 3.10+, GCC/G++ with C11 and C++17 support.

## Usage

Compile your target with debug symbols and no position-independent code:

```bash
gcc -g -O0 -no-pie -o target target.c
```

Run the debugger:

```bash
./bin/bugme ./target
```

Find function addresses with objdump:

```bash
objdump -d target | grep '<main>\|<your_function>'
```

### Commands

| Command | Description |
|--------|-------------|
| `brk <addr>` | Set a breakpoint at hex address (e.g. `brk 0x401189`) |
| `cnt` | Continue execution |
| `regs` | Print all CPU registers |
| `q` | Kill the tracee and quit |

### Example session

```
$ ./bin/bugme ./bin/tests/testdbg
bugme> brk 0x401189
bugme> cnt
Breakpoint hit at 0x401189
bugme> regs
rax        0x401189
rip        0x401189
rdi        0x3
rsi        0x4
...
bugme> cnt
add(3, 4) = 7
factorial(5) = 120
sum = 150
done
Tracee exited with code: 0
```

## What I learned

- How `ptrace` works and what it actually exposes — registers, memory, signals, syscalls
- How breakpoints are implemented at the hardware level (`int 3` / `0xcc`)
- Why the instruction pointer is off by one when a breakpoint fires, and how to rewind it
- C++ OOP in a real context — classes, constructors, `enum class`, `std::unordered_map`, `std::optional`, RAII
- How to mix C and C++ in the same project using `extern "C"`
- What ASLR is and why `-no-pie` matters when setting breakpoints by address
- What the `W*` macros (`WIFEXITED`, `WIFSTOPPED`, `WSTOPSIG` etc.) actually unpack from `waitpid`'s status integer

## Planned

- Single stepping (`step` command)
- Memory inspection (`mem <addr>`)
- Re-enabling breakpoints after stepping over them
- DWARF debug info parsing — set breakpoints by function name instead of raw address
- A TUI with a register panel, code view, and command palette
