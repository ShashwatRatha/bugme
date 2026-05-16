#include "registers.hpp"

#include <sys/user.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <stdexcept>

#include "ptraceWrappers.h"

static const struct {
  Regs regIdx;
  const char *name;
  size_t offset;
} RegDesc[] = {{Regs::rax, "rax", offsetof(user_regs_struct, rax)},
               {Regs::rbx, "rbx", offsetof(user_regs_struct, rbx)},
               {Regs::rcx, "rcx", offsetof(user_regs_struct, rcx)},
               {Regs::rdx, "rdx", offsetof(user_regs_struct, rdx)},
               {Regs::rdi, "rdi", offsetof(user_regs_struct, rdi)},
               {Regs::rsi, "rsi", offsetof(user_regs_struct, rsi)},
               {Regs::rsp, "rsp", offsetof(user_regs_struct, rsp)},
               {Regs::rbp, "rbp", offsetof(user_regs_struct, rbp)},
               {Regs::r8, "r8", offsetof(user_regs_struct, r8)},
               {Regs::r9, "r9", offsetof(user_regs_struct, r9)},
               {Regs::r10, "r10", offsetof(user_regs_struct, r10)},
               {Regs::r11, "r11", offsetof(user_regs_struct, r11)},
               {Regs::r12, "r12", offsetof(user_regs_struct, r12)},
               {Regs::r13, "r13", offsetof(user_regs_struct, r13)},
               {Regs::r14, "r14", offsetof(user_regs_struct, r14)},
               {Regs::r15, "r15", offsetof(user_regs_struct, r15)},
               {Regs::rip, "rip", offsetof(user_regs_struct, rip)}};

Registers::Registers(pid_t pid) : mPid(pid), mRegs{} {}

void Registers::getRegs() {
  if (ptGetRegs(mPid, &mRegs) == -1) {
    std::perror("registers");
    exit(1);
  }
}

void Registers::setRegs(user_regs_struct &regs) {
  if (ptSetRegs(mPid, &regs) == -1) {
    std::perror("registers");
    exit(1);
  }
  mRegs = regs;
}

const std::optional<Regs> Registers::getRegister(
    const std::string &regName) const {
  for (const auto &reg : RegDesc) {
    if (reg.name == regName) {
      return reg.regIdx;
    }
  }

  return std::nullopt;
}

uint64_t Registers::getRegisterValue(Regs regIdx) const {
  for (auto reg : RegDesc) {
    if (regIdx == reg.regIdx) {
      return *(uint64_t *)((uint8_t *)&mRegs + reg.offset);
    }
  }

  throw std::runtime_error("Unknown Register");
}

void Registers::setRegister(const Regs regIdx, uint64_t value) {
  for (auto reg : RegDesc) {
    if (regIdx == reg.regIdx) {
      *(uint64_t *)((uint8_t *)&mRegs + reg.offset) = value;
      setRegs(mRegs);
      return;
    }
  }

  throw std::runtime_error("Unknown Register");
}

void Registers::renderRegs() const {
  printf("%-6s 0x%018lx\n", "rip", getRegisterValue(Regs::rip));
  for (auto i = 0; i < 4; i++) {
    printf("%-6s 0x%-22.18lx", RegDesc[4 * i].name,
           getRegisterValue(RegDesc[4 * i].regIdx));
    printf("%-6s 0x%-22.18lx", RegDesc[4 * i + 1].name,
           getRegisterValue(RegDesc[4 * i + 1].regIdx));
    printf("%-6s 0x%-22.18lx", RegDesc[4 * i + 2].name,
           getRegisterValue(RegDesc[4 * i + 2].regIdx));
    printf("%-6s 0x%-22.18lx", RegDesc[4 * i + 3].name,
           getRegisterValue(RegDesc[4 * i + 3].regIdx));
    putchar('\n');
  }
}
