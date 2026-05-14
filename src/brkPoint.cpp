#include "brkPoint.hpp"

#include "ptraceWrappers.h"

BreakPoint::BreakPoint(pid_t pid, std::uint64_t addr)
    : mPid(pid), mAddr(addr), mReadByte(0), mEnabled(false) {}

bool BreakPoint::isEnabled() const { return mEnabled; }

std::uint64_t BreakPoint::getAddr() const { return mAddr; }

void BreakPoint::enableBP() {
  auto readWord = ptReadMem(mPid, mAddr);
  mReadByte = static_cast<std::uint8_t>(readWord & 0xff);
  auto writeWord = ((readWord & ~(0xffULL)) | 0xcc);
  ptWriteMem(mPid, mAddr, writeWord);
  mEnabled = true;
}

void BreakPoint::disableBP() {
  auto readWord = ptReadMem(mPid, mAddr);
  auto writeWord =
      ((readWord & ~(0xffULL)) | static_cast<std::uint64_t>(mReadByte));
  ptWriteMem(mPid, mAddr, writeWord);
  mEnabled = false;
}
