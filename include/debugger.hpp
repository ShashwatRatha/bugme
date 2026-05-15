#ifndef DEBUG_H_
#define DEBUG_H_

#include <sys/types.h>

#include <string>
#include <unordered_map>

#include "brkPoint.hpp"
#include "elfParser.hpp"
#include "registers.hpp"

class Debugger {
 public:
  Debugger(const char *program, char *const argv[]);
  void run();
  void cnt(int signal);
  void getRegs();
  void setBP(uint64_t addr);
  void setBP(const std::string &symName);
  void setRegister(Regs reg, uint64_t value);

 private:
  pid_t mPid;
  Registers mRegs;
  ElfParser mElf;
  std::unordered_map<uint64_t, BreakPoint> mBrkPoints;

  void handleTRAP();
  void handleCommand(std::string &line);
  void wait();
  uint64_t getRIP();
  void setRIP(uint64_t addr);
};

#endif
