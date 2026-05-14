#include "registers.hpp"

#include <sys/user.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

#include "ptraceWrappers.h"

struct Register {
  Regs regIdx;
  const char *name;
  size_t offset;
};

static const Register RegDesc[] = {
    {Regs::rax, "rax", offsetof(user_regs_struct, rax)},
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

void Registers::getRegs() { ptGetRegs(mPid, &mRegs); }

void Registers::setRegs(user_regs_struct &regs) {
  ptSetRegs(mPid, &regs);
  mRegs = regs;
}

uint64_t Registers::getRegister(Regs regIdx) const {
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
      return;
    }
  }

  throw std::runtime_error("Unknown Register");
}

void Registers::renderRegs() const {
  for (const auto &reg : RegDesc) {
    uint64_t val = *(uint64_t *)((uint8_t *)&mRegs + reg.offset);
    printf("%-10s 0x%lx\n", reg.name, val);
  }
}
