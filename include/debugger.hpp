#ifndef DEBUG_H_
#define DEBUG_H_

#include <sys/types.h>

#include <cstdint>
#include <string>
#include <unordered_map>

#include "brkPoint.hpp"
#include "elfParser.hpp"
#include "registers.hpp"

class Debugger {
 public:
  Debugger(const char *program, char *const argv[]);
  void run();

 private:
  bool mPExited;
  pid_t mPid;
  Registers mRegs;
  ElfParser mElf;
  std::unordered_map<uint64_t, BreakPoint> mBrkPoints;
  struct MemRegion {
    std::uint64_t startAddr;
    std::uint64_t endAddr;
    bool r, w, x;
  };

  std::optional<MemRegion> getRegion(const uint64_t &addr);
  uint64_t parseAddr(const std::string &addr);
  void cnt(int signal);
  void getRegs();
  void handleCommand(std::string &line);
  void handleTRAP();
  void memRead(std::uint64_t addr, std::uint16_t n = 64);
  void memWrite(std::uint64_t addr, std::uint64_t value);
  void setBP(uint64_t addr);
  void setRegister(const Regs &reg, std::uint64_t value);
  void wait();
};

#endif
