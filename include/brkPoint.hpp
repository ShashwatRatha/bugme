#ifndef BRKPOINT_HPP_
#define BRKPOINT_HPP_

#include <sys/types.h>

#include <cstdint>

/**
 * @brief Manages a single software breakpoint inserted into the tracee's memory
 * space.
 *
 * This class abstracts the cycle of saving the original instruction instruction
 * byte, patching it with an x86 INT3 opcode (0xCC) to interrupt execution, and
 * restoring the original byte upon disabling or stepping through.
 */
class BreakPoint {
 public:
  /**
   * @brief Constructs a BreakPoint instance for a specific address.
   * * @param pid The process ID of the tracee/target process.
   * @param addr The virtual memory address where the breakpoint should be
   * placed.
   */
  BreakPoint(pid_t pid, std::uint64_t addr);

  BreakPoint(const BreakPoint&) = default;
  BreakPoint& operator=(const BreakPoint&) = default;
  BreakPoint(BreakPoint&&) = default;
  BreakPoint& operator=(BreakPoint&&) = default;
  ~BreakPoint() = default;

  /** * @brief Checks whether the breakpoint is currently patched into the
   * tracee memory.
   * @return true if the INT3 instruction is active at the address, false
   * otherwise.
   */
  bool isEnabled() const;

  /** * @brief Gets the virtual address associated with this breakpoint.
   * @return The target execution address tracking this breakpoint.
   */
  std::uint64_t getAddr() const;

  /** * @brief Inserts the INT3 (0xCC) instruction at the target breakpoint
   * address.
   * * Reads the existing 64-bit word at the location, backs up the lower-order
   * byte into internal storage (`mReadByte`) if not already enabled, updates
   * the byte to `0xCC`, and writes the modified word back to the tracee's
   * memory space.
   */
  void enableBP();

  /** * @brief Restores the original instruction byte at the target breakpoint
   * address.
   * * Overwrites the active INT3 (0xCC) opcode by shifting the saved
   * `mReadByte` back into the lower byte of the target memory word, thereby
   * reverting the code segment to its pristine state.
   */
  void disableBP();

 private:
  pid_t mPid = -1;  ///< Target process ID.
  bool mEnabled =
      false;  ///< Tracking state flag for the breakpoint activation.
  std::uint64_t mAddr = 0;  ///< Target instruction memory pointer.
  std::uint8_t mReadByte =
      0;  ///< Storage for the cached original instruction byte.
};

#endif
