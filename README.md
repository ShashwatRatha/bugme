# BugMe

A Linux userspace debugger built from scratch using `ptrace`, written in C and C++.

Designed to expose how debuggers like `gdb` actually work at a low level — software breakpoints, register inspection, memory access, symbol resolution via ELF parsing, call stack unwinding, and disassembly via Capstone.

---

## Core Concepts

### ptrace

The debugger relies on the Linux `ptrace` syscall as its fundamental mechanism to observe and control the tracee:

- **Process control:** Spawns and attaches to a child process via `PTRACE_TRACEME`
- **Execution control:** Continues and single-steps via `PTRACE_CONT` and `PTRACE_SINGLESTEP`
- **Register access:** Reads and writes the full CPU register file via `PTRACE_GETREGS` and `PTRACE_SETREGS`
- **Memory access:** Reads and writes tracee memory via `PTRACE_PEEKDATA` and `PTRACE_POKEDATA`

### Breakpoints

Software breakpoints work by rewriting tracee instructions at runtime:

1. Read the 8-byte word at the target address
2. Save the low byte, replace it with `0xcc` (`int 3`), write back
3. When the CPU executes `0xcc`, it raises `SIGTRAP` — the tracee stops
4. The debugger restores the original byte, rewinds `RIP` by 1, single-steps the original instruction, then replants `0xcc`

This keeps breakpoints armed across multiple hits. Fully encapsulated in the `BreakPoint` class.

### Registers

- Register state is fetched from the tracee after every stop event and cached in `mRegs`
- Individual registers are accessed by offset into `user_regs_struct` via `offsetof`, keyed by an `enum class Regs`
- Reads and writes go through `PTRACE_GETREGS` / `PTRACE_SETREGS`
- String-to-register resolution (`"rax"` → `Regs::rax`) is handled by scanning a static descriptor table

### ELF and Symbol Resolution

- Parses `.symtab` and `.dynsym` sections via `libelf` to resolve symbol names to addresses
- Maintains a forward map (`name → address`) for `brk main` style breakpoints
- Maintains a reverse map (`address → name + size`) using `std::map` for `upper_bound` lookups, used by backtrace and disassembly annotation
- PIE-aware: reads `/proc/<pid>/maps` to find the runtime load base and adjusts all addresses accordingly
- Exposes the raw `.text` section bytes and base address for disassembly

### Backtrace

Walks the x86-64 frame pointer chain using `ptReadMem`:

```
current rbp → *(rbp)     = saved rbp of caller  (previous frame)
            → *(rbp + 8) = return address        (rip of caller)
```

Each return address is resolved to a symbol name via reverse lookup in `mAddrMap`. Terminates when the saved `rbp` is 0 or a ptrace read fails.

### Disassembly

- Loads the entire `.text` section from the ELF file at startup via `ElfParser::loadTextSection`
- Passes raw bytes to Capstone (`cs_disasm`) once, stores decoded `Instruction` structs
- Builds an `unordered_map<address, index>` for O(1) lookup at render time
- `renderDisassembly` prints a window of instructions around a target address with:
  - `▶` marking the current `RIP`
  - `` marking addresses with active breakpoints
- For PIE binaries, file offsets are adjusted by the runtime load base at display time

### Memory Validation

All `memr` and `memw` operations are validated against `/proc/<pid>/maps` before issuing any ptrace call:

- Confirms the address is mapped
- Checks read/write permission bits from the `perms` field
- Rejects writes that would cross a region boundary

---

## Architecture

Strict boundary between raw C syscall wrappers and C++ application logic:

```
main.cpp
  └── Debugger            CLI loop, command dispatch, signal handling
        ├── BreakPoint    int3 plant/restore, re-arm after step-over
        ├── Registers     register read/write, string-to-enum resolution
        ├── ElfParser     symbol tables, PIE load address, .text section
        ├── Disassembler  Capstone wrapper, instruction decode
        └── ptraceWrappers.c   raw ptrace/fork/exec/waitpid calls (C)
```

---

## Building

### Requirements

- CMake >= 3.10
- GCC / G++ with C11 and C++17 support
- `libelf` (`sudo apt install libelf-dev`)
- `libcapstone` (`sudo apt install libcapstone-dev`)

### Compilation

```bash
scripts/build
```

Output: `bin/bugme-cli`

---

## Usage

Compile your target with debug symbols and no optimisations:

```bash
gcc -g -O0 -o target target.c
```

For non-PIE (recommended for address-based breakpoints):

```bash
gcc -g -O0 -no-pie -o target target.c
```

Run under the debugger:

```bash
./bin/bugme-cli ./target
```

### Commands

| Command | Description |
|---------|-------------|
| `cnt` | Continue execution |
| `step` | Single-step one machine instruction |
| `brk <addr\|symbol>` | Set a breakpoint at an address or symbol |
| `regs` | Print all CPU registers |
| `regw <reg> <value>` | Write a hex value into a register |
| `memr <addr> [n]` | Read n bytes from tracee memory (default 64) |
| `memw <addr> <value>` | Write a 64-bit word to tracee memory |
| `disas [addr\|symbol]` | Disassemble around address or current RIP |
| `bt` | Print call stack backtrace |
| `help` | Print command reference |
| `q` | Kill tracee and exit |

### Address Expressions

All address arguments are parsed by `Debugger::parseAddr`:

- **Hex address:** `brk 0x401189`
- **Symbol name:** `brk main`
- **Symbol + decimal offset:** `brk main+16`
- **Symbol - decimal offset:** `brk main-8`

PIE binaries are handled transparently — the load base is read from `/proc/<pid>/maps` and added automatically.

---

## Execution Flow

```
ptSpawn()
  fork() → child: PTRACE_TRACEME + execvp()
  parent: waitpid() → tracee stopped on initial SIGTRAP

Debugger constructor
  loadDisassembly() → disassemble .text, build index map
  loadCommands()    → populate command dispatch table

CLI loop
  read line → tokenise → dispatch via mCommands map

On SIGTRAP (breakpoint hit):
  disable breakpoint → restore original byte
  rewind RIP by 1
  single-step original instruction
  re-enable breakpoint
  return to CLI prompt

On cnt:
  ptContinue → waitpid → handle signal
```

---

## Limitations

- Single-threaded tracees only — ptrace behaviour with multi-threaded targets is not handled
- No DWARF support — breakpoints and backtraces work by address and frame pointer, not source lines
- Frame pointer walking requires `-fno-omit-frame-pointer` (implied by `-O0`) — optimised binaries may produce incomplete backtraces
- No conditional breakpoints or watchpoints

## Planned

- TUI — register panel, scrollable disassembly view with RIP tracking, command palette
- DWARF parsing — source-level stepping, `brk file.c:42` style breakpoints
- Hardware breakpoints via debug registers (DR0–DR7)
- Watchpoints — break on memory write to an address
- Conditional breakpoints — evaluate an expression on each hit
