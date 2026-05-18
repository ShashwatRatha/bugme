#ifndef REGISTERS_HPP_
#define REGISTERS_HPP_

#include <sys/types.h>
#include <sys/user.h>

#include <cstdint>
#include <optional>
#include <string>

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

class Registers {
 public:
  Registers(pid_t pid);

  void getRegs();
  void setRegs(user_regs_struct& regs);
  const std::optional<Regs> getRegister(const std::string& regName) const;
  uint64_t getRegisterValue(Regs reg) const;
  void setRegister(const Regs reg, uint64_t value);
  void renderRegs() const;

 private:
  pid_t mPid;
  user_regs_struct mRegs;
};

#endif
