#ifndef REGISTERS_HPP_
#define REGISTERS_HPP_

#include <sys/types.h>
#include <sys/user.h>

#include <cstdint>
#include <optional>
#include <string>

/**
 * @brief Enumerated type identifying individual supported x86_64 architecture
 * registers.
 */
enum class Regs {
  rax,
  rbx,
  rcx,
  rdx,
  rdi,
  rsi,
  rsp,
  rbp,
  r8,
  r9,
  r10,
  r11,
  r12,
  r13,
  r14,
  r15,
  eflags,
  rip
};

/**
 * @brief Class managing hardware register inspection, synchronization, and
 * modification.
 */
class Registers {
 public:
  /**
   * @brief Constructs a register controller instance tied to a target process
   * tracee.
   * @param pid Process ID tracking the traced process.
   */
  Registers(pid_t pid);

  /**
   * @brief Synchronizes the internal `mRegs` cache by calling `PTRACE_GETREGS`
   * on the tracee.
   */
  void getRegs();

  /**
   * @brief Updates the internal structure cache and commits these modifications
   * back to the tracee CPU.
   * @param regs Complete `user_regs_struct` container tracking updated states.
   */
  void setRegs(user_regs_struct& regs);

  /**
   * @brief Converts a text string token into its matching register enumeration
   * index.
   * @param regName String name corresponding to target register (e.g. "rax",
   * "rip").
   * @return Matching Regs enumeration symbol identifier on match, std::nullopt
   * on bad tokens.
   */
  const std::optional<Regs> getRegister(const std::string& regName) const;

  /**
   * @brief Fetches the current value of a target register from the internal
   * cache.
   * @param reg Enumerated architecture register indicator.
   * @return The 64-bit value currently stored inside that register register
   * tracker.
   */
  uint64_t getRegisterValue(Regs reg) const;

  /**
   * @brief Modifies the value of a target register in the cache and commits the
   * change to the tracee.
   * @param reg Enumerated architecture register indicator.
   * @param value New 64-bit data payload value to write.
   */
  void setRegister(const Regs reg, uint64_t value);

  /**
   * @brief Formats and prints the current states of all primary x86_64
   * registers onto standard console output.
   */
  void renderRegs() const;

 private:
  pid_t mPid;  ///< Target process identifier tracked for system calls.
  user_regs_struct mRegs;  ///< Low-level structure cache housing active
                           ///< architectural register values.
};

#endif
