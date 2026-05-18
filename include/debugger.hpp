#ifndef DEBUG_HPP_
#define DEBUG_HPP_

#include <sys/types.h>

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "brkPoint.hpp"
#include "disassembler.hpp"
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
  std::unordered_map<std::string,
                     std::function<void(const std::vector<std::string> &)>>
      mCommands;
  Disassembler mDisas;
  std::vector<Disassembler::Instruction> mDisasInstructions;
  std::unordered_map<std::uint64_t, int> mAddrInsn;
  struct MemRegion {
    std::uint64_t startAddr;
    std::uint64_t endAddr;
    bool r, w, x;
  };

  std::optional<MemRegion> getRegion(const uint64_t &addr);
  uint64_t parseAddr(const std::string &addr);
  void cnt(int signal = 0);
  void getRegs();
  void handleCommand(std::string &line);
  void handleTRAP();
  void memRead(std::uint64_t addr, std::size_t n = 64);
  void memWrite(std::uint64_t addr, std::uint64_t value);
  void setBP(uint64_t addr);
  void setRegister(const Regs &reg, std::uint64_t value);
  void wait();
  void backTrace();
  void loadCommands();
  void loadDisassembly();
  void renderDisassembly(const uint64_t &addr, const size_t &num = 16);
};

#endif
