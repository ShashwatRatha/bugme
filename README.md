# BugMe

A minimal Linux userspace debugger built from scratch using `ptrace`, written in C and C++.

The project is designed to expose how debuggers like `gdb` actually work at a low level — including software breakpoints, register inspection, memory access, and symbol resolution via ELF parsing.

## Core Concepts

### Ptrace

The debugger relies on the Linux `ptrace` system call as its fundamental mechanism to observe and control the target process:

* **Process Lifecycles:** Spawns and tracks a child process via `PTRACE_TRACEME`.
* **Execution Control:** Manipulates runtime states using `PTRACE_CONT` and `PTRACE_SINGLESTEP`.
* **Register Modification:** Inspects and overwrites CPU architectures via `PTRACE_GETREGS` and `PTRACE_SETREGS`.
* **Memory Access:** Directly manipulates target memory spaces via `PTRACE_PEEKDATA` and `PTRACE_POKEDATA`.

### Breakpoints

Software breakpoints are achieved by physically rewriting tracee process instructions at runtime:

1. The debugger reads the target instruction at a specified address.
2. It replaces the first byte of that instruction with `0xCC` (`int 3`).
3. When the CPU hits the trap, execution pauses and a `SIGTRAP` signal wakes up the debugger.
4. **On Trap:** The debugger restores the original code byte, rewinds the Instruction Pointer (`RIP`) by 1 byte, steps over the original instruction, and then reinserts the `0xCC` breakpoint to keep it armed.

This state machine is fully encapsulated within the `BreakPoint` class.

### Register Model

* **Caching:** Register states are cached locally inside the debugger layer after every process stop event.
* **Granular Access:** Targeted fields are parsed via accurate byte offsets into the Linux `user_regs_struct`.
* **Decoupling:** Read and write triggers are clean wrappers isolating systemic side-effects, managed inside the `Registers` class.

### ELF & Symbol Resolution

* Parses the binary's ELF Symbol Table to resolve human-readable symbol strings to physical memory addresses.
* Features Position-Independent Executable (PIE) aware address translations.
* Extracts the live process load-base dynamically by reading `/proc/<pid>/maps`.

This component is managed entirely by the `ElfParser` class.

---

## Architecture

The project maintains a strict boundary between low-level system execution wrappers written in C and high-level structural orchestration classes written in modern C++:

```
main.cpp
  └── Debugger          Central orchestrator; executes the user CLI loop and handles OS signals
        ├── BreakPoint  Manages software breakpoint insertion, modification, and restoration states
        ├── Registers   Abstracts CPU registers, mapping string identifiers to architectural registers
        ├── ElfParser   Parses ELF data headers, resolving symbols to memory locations (PIE-aware)
        └── ptraceWrappers (C) Thin procedural wrappers isolating direct Linux syscall interfaces

```

---

## Building

### Requirements

* **CMake** >= 3.10
* **GCC / G++** compilers supporting C11 and C++17 standards
* **libelf** runtime development library (used for symbol parsing)

The build engine utilizes `pkg-config` to locate, verify, and dynamically link against your environment's system `libelf` binaries.

### Compilation

```bash
mkdir build && cd build
cmake ..
make

```

The compiled output is generated at `bin/bugme`.

---

## Usage

Compile your target program with debugging symbols explicitly included and optimizations disabled (`-O0`):

```bash
gcc -g -O0 -o target target.c

```

Launch your application inside the debugger:

```bash
./bin/bugme ./target

```

### Command Reference

| Command | Description |
| --- | --- |
| `cnt` | Continue execution |
| `step` | Execute a single machine instruction |
| `brk <addr / symbol>` | Set a software breakpoint at a hex address, a symbol name, or an offset |
| `regs` | Print snapshot values of all active CPU registers |
| `regw <reg> <value>` | Write an explicit hexadecimal value into a target register |
| `memr <addr> [n]` | Read `n` bytes of memory starting at a specified address |
| `memw <addr> <value>` | Write a value into a specific target memory address |
| `help` | Display the command help matrix |
| `q` | Terminate the tracee process and exit the debugger |

### Address Expressions

The interpreter resolves expressions using `Debugger::parseAddr` to correctly map layout dependencies under PIE environments:

* **Raw Hex Endpoints:** `brk 0x401000`
* **Symbol Strings:** `brk main`
* **Offset Math:** `brk main+16` or `brk 0x401000-8`

### Memory Validation Model

All memory access actions are checked against `/proc/<pid>/maps` records before executing actual reads or writes. This layer:

* Confirms target regions exist inside mapped spaces.
* Validates explicit permission masks (Read/Write attributes).
* Eliminates out-of-bounds `ptrace` system faults or sudden debugger runtime panics.

---

## Execution Flow

```
[ptSpawn()] Fork & setup tracee via PTRACE_TRACEME
   │
   ▼
[Debugger] Blocks & traps initial tracee initialization signal
   │
   ▼
[CLI Loop] Awaits user command input (e.g., set breakpoint, continue)
   │
   ▼
[SIGTRAP] Intercepted breakpoint trigger:
   ├── Detect target breakpoint mapping matches
   ├── Rewind CPU Instruction Pointer (RIP)
   ├── Step past original instruction safely
   └── Re-arm breakpoint with 0xCC trap

```

---

## Limitations & Future Work

### Limitations

* **Disassembler:** Context command frameworks exist but structural outputs are unimplemented.
* **Stack Maps:** Backtrace call stack unwinding tracking is missing.
* **DWARF:** Source-level debugging using raw lines or file scopes is not yet integrated.
* **Concurrency:** Limited to single-threaded runtime targets.
* **Interface:** Restricted strictly to standard CLI processing.

### Planned Features

* **DWARF Integration:** Native source-level stepping and expression lookups.
* **Disassembly Engine:** In-line machine instruction printouts alongside execution pointers.
* **Stack Unwinding:** Stack tracking maps to cleanly recreate complete backtraces.
* **Conditional Actions:** Evaluate expression logic parameters on breakpoint hits.
* **Hardware Tracing:** Use debug registers to configure hardware breakpoints and watchpoints.
* **TUI Framework:** Terminal visual dashboard presenting register tables and live code states simultaneously.

---

## Learning Value

Developing this engine provided hands-on experience with:

* Practical applications, boundaries, and low-level mechanics of the Linux `ptrace` API.
* Real-world software breakpoint traps, hardware behaviors, and pipeline correction techniques.
* ELF layout parsing, symbol string tables, and translation schemes.
* Inspecting virtual memory layout attributes via the `/proc` subsystem.
* Low-level CPU manipulation across active registers.
* Separating raw procedural system layers from maintainable, modern object-oriented codebases.
