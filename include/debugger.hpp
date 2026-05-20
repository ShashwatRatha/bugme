#ifndef DEBUG_HPP_
#define DEBUG_HPP_

#include <sys/types.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "brkPoint.hpp"
#include "disassembler.hpp"
#include "elfParser.hpp"
#include "registers.hpp"

/**
 * @brief Core engine driving the interactive debugging environment.
 *
 * Coordinates tracee instantiation via fork/exec, event handling loop via
 * waitpid, user CLI dispatch loops, hardware/software context inspection, and
 * instruction disassembling capabilities.
 */
class Debugger {
 public:
  /**
   * @brief Initializes the debugger instance, forks, and readies the target
   * tracee.
   * * Forks the current process. The child process invokes `PTRACE_TRACEME` and
   * execs the program, while the parent constructor stores state and maps the
   * command CLI router.
   * * @param program Path to the executable binary.
   * @param argv Null-terminated array of arguments to be forwarded to the
   * binary execution.
   */
  Debugger(const char *program, char *const argv[]);

  /**
   * @brief Enters the main execution controller loop.
   * * Synchronizes initialization by waiting for the child's initial SIGTRAP
   * trap, populates symbol/disassembly maps, and drops into an interactive user
   * prompt line reader until the tracee exits or terminates.
   */
  void run();

 private:
  bool mPExited;    ///< Flags if the tracee process has permanently exited.
  pid_t mPid;       ///< Process ID of the spawned tracee.
  Registers mRegs;  ///< Handles synchronization and access to tracee registers.
  ElfParser mElf;   ///< Responsible for parsing section headers, symbols, and
                    ///< PIE bases.
  std::unordered_map<uint64_t, BreakPoint>
      mBrkPoints;  ///< Registry mapping addresses to active Breakpoint objects.

  /// CLI action router executing lambda mappings from terminal tokens.
  std::unordered_map<std::string,
                     std::function<void(const std::vector<std::string> &)>>
      mCommands;

  Disassembler mDisas;  ///< Capstone engine instance pointer wrapper.
  std::vector<Disassembler::Instruction>
      mDisasInstructions;  ///< Decoded instructions container cache.
  std::unordered_map<std::uint64_t, int>
      mAddrInsn;  ///< Maps virtual addresses to instruction vector indices.

  /**
   * @brief Defines layout mapping metadata for virtual memory regions parsed
   * from procfs.
   */
  struct MemRegion {
    std::uint64_t startAddr;  ///< Start bounds of segment.
    std::uint64_t endAddr;    ///< End bounds of segment.
    bool r, w, x;             ///< Page permission bitflags.
  };

  /**
   * @brief Inspects `/proc/<pid>/maps` to verify presence and permissions of an
   * address space region.
   * @param addr Memory location to test.
   * @return A populated MemRegion struct if matching segment found,
   * std::nullopt otherwise.
   */
  std::optional<MemRegion> getRegion(const uint64_t &addr);

  /**
   * @brief Tokenizes input numeric expressions or resolves symbols into
   * accurate absolute addresses.
   * * Correctly shifts addresses automatically based on load offsets if the
   * binary is PIE compliant.
   * @param addr Hexadecimal string literal starting with "0x" or raw symbol
   * names (e.g. "main").
   * @return Resolved absolute 64-bit address target pointer.
   */
  uint64_t parseAddr(const std::string &addr);

  /**
   * @brief Resumes child execution, handling intermediate single-stepping over
   * active breakpoints.
   * * If the current `$rip` coincides with an active breakpoint address, this
   * method will automatically disable the breakpoint, step forward by exactly
   * one instruction, re-enable the breakpoint, and then continue full
   * execution.
   * * @param signal Signal payload to pass forward to the tracee (defaults to
   * 0).
   */
  void cnt(int signal = 0);

  /**
   * @brief Forces the underlying Registers instance to pull fresh CPU states
   * via ptrace.
   */
  void getRegs();

  /**
   * @brief Parses and dispatches raw terminal input strings into mapped
   * internal command callbacks.
   * @param line Raw input read from stdin.
   */
  void handleCommand(std::string &line);

  /**
   * @brief Evaluates breakpoint intersections and rewinds the program counter
   * upon triggering an INT3 trap.
   * * Because x86 execution stops *after* executing the INT3 instruction, this
   * method checks if
   * `$rip - 1` matches an active breakpoint. If it does, it winds `$rip` back
   * by 1 byte to point to the correct original instruction start.
   */
  void handleTRAP();

  /**
   * @brief Dumps a hex-formatted slice of the tracee's virtual memory to
   * stdout.
   * @param addr Source memory base address to start tracking.
   * @param n Total number of continuous bytes to print.
   */
  void memRead(std::uint64_t addr, std::size_t n = 64);

  /**
   * @brief Overwrites a single 64-bit word value at a specified virtual memory
   * target.
   * @param addr Destination address inside the tracee's memory space.
   * @param value Raw 64-bit data value payload to insert.
   */
  void memWrite(std::uint64_t addr, std::uint64_t value);

  /**
   * @brief Registers and enables a new software breakpoint at the specified
   * target address.
   * @param addr Absolute memory address where the code patch must be applied.
   */
  void setBP(uint64_t addr);

  /**
   * @brief Mutates a specific target register to a new given value state.
   * @param reg Enumerated architecture register indicator.
   * @param value 64-bit data payload value to write.
   */
  void setRegister(const Regs &reg, std::uint64_t value);

  /**
   * @brief Blocks execution until the tracee process encounters a
   * status-changing event signal.
   * * Intercepts standard stop signals, tracking whether the target process
   * terminated, exited cleanly, or received an expected `SIGTRAP`.
   */
  void wait();

  /**
   * @brief Generates a comprehensive stack backtrace via call frame pointer
   * traversal.
   * * Progressively unwinds execution layers by walking through stacked `$rbp`
   * linkages and looking up active instruction return addresses via symbol
   * table metadata lookup.
   */
  void backTrace();

  /**
   * @brief populates internal command dictionary routing mapping user string
   * tokens to CLI methods.
   */
  void loadCommands();

  /**
   * @brief Parses out relevant assembly chunks out from `.text` boundaries to
   * initialize instruction tracking maps.
   */
  void loadDisassembly();

  /**
   * @brief Renders lines of assembly code with symbol tracking information onto
   * standard console out.
   * @param addr Pivoting reference instruction pointer address around which
   * disassembly highlights focus.
   * @param num Total lines/count window of surrounding instructions to print.
   */
  void renderDisassembly(const uint64_t &addr, const size_t &num = 16);
};

#endif
