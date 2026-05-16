# BugMe

A Linux debugger built from scratch in C and C++, using `ptrace`. Supports setting breakpoints, inspecting registers, and stepping through execution of any ELF binary.

Built as a learning project to understand how debuggers like gdb and lldb actually work under the hood.

## How it works

The core mechanism is the Linux `ptrace` syscall, which lets one process (the debugger) observe and control another (the tracee). A breakpoint is just one byte — `0xcc` (`int 3`) — written into the tracee's code at the target address. When the CPU executes it, a `SIGTRAP` is raised, the process pauses, and the debugger wakes up. It then restores the original byte, rewinds the instruction pointer by one, and hands control back to the user.

Register reads and writes go through `PTRACE_GETREGS` / `PTRACE_SETREGS`, which snapshot the full CPU register state of the tracee at any point before it exits. The register state is cached upon each call to _ptGetRegs()_, which is used to show the last updated register values after the tracee exits.

## Architecture

The project is written in both C and C++:

- **C** (`ptraceWrappers.c`) — thin wrappers around raw `ptrace` syscalls, `fork`, `execvp`, `waitpid`
- **C++** (`Debugger`, `BreakPoint`, `Registers`, `ElfParser` classes) — owns all state and logic, calls into the C layer

```
main.cpp
  └── Debugger          owns everything, runs the command loop
        ├── Registers   fetches/writes CPU registers via ptrace
        ├── BreakPoint  plants/restores int3 at a given address
        └── ElfParser   finds loading address of the elf and finds symbols in it
```

## Building

```bash
scripts/build
```

Requires libelf, CMake 3.10+, GCC/G++ with C11 and C++17 support.

## Usage

Compile your target with debug symbols and -O0 enabled:

```bash
gcc -g -O0 -o target target.c
```

Run the debugger:

```bash
./bin/bugme ./target
```

Find function addresses with objdump:

### Commands

| Command | Description |
|--------|-------------|
| `brk <addr or symbol>` | Set a breakpoint at hex address (e.g. `brk 0x401189`) or at a symbol (e.g. `brk main`) |
| `cnt` | Continue execution |
| `regs` | Print all CPU registers |
| `step` | Step through single instruction |
| `q` | Kill the tracee and quit |

### Example session

```
$ bin/bugme bin/tests/testdbg
bugme> brk factorial
bugme> cnt
starting
add(3, 4) = 7
Breakpoint hit at 0x56fc1ca651a1
bugme> regs
rip    0x00000056fc1ca651a1
rax    0x00000000000000000e    rbx    0x0000007ffee72bb728    rcx    0x000000000000000000    rdx    0x000000000000000000    
rdi    0x000000000000000005    rsi    0x00000056fc4b93f2a0    rsp    0x0000007ffee72bb5c8    rbp    0x0000007ffee72bb600    
r8     0x000000000000000064    r9     0x000000000000000000    r10    0x000000000000000000    r11    0x000000000000000202    
r12    0x000000000000000001    r13    0x000000000000000000    r14    0x00000056fc1ca67db0    r15    0x00000072feb4d24000    
bugme> step
bugme> regs
rip    0x00000056fc1ca651a5
rax    0x00000000000000000e    rbx    0x0000007ffee72bb728    rcx    0x000000000000000000    rdx    0x000000000000000000    
rdi    0x000000000000000005    rsi    0x00000056fc4b93f2a0    rsp    0x0000007ffee72bb5c8    rbp    0x0000007ffee72bb600    
r8     0x000000000000000064    r9     0x000000000000000000    r10    0x000000000000000000    r11    0x000000000000000202    
r12    0x000000000000000001    r13    0x000000000000000000    r14    0x00000056fc1ca67db0    r15    0x00000072feb4d24000    
bugme> cnt
factorial(5) = 120
sum = 150
done
Tracee exited with code: 0
bugme> q 

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

- Memory inspection (`mem <addr>`)
- Re-enabling breakpoints after stepping over them
- DWARF debug info parsing — set breakpoints by function name instead of raw address
- A TUI with a register panel, code view, and command palette
