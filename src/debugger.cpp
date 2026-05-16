#include "debugger.hpp"

#include <sys/types.h>
#include <sys/wait.h>

#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "brkPoint.hpp"
#include "ptraceWrappers.h"
#include "registers.hpp"

#define HELP_TXT                                                        \
  "cnt                     continue execution\n"                        \
  "step                    single-step one instruction\n"               \
  "brk <addr|symbol>       set a breakpoint\n"                          \
  "mem <addr> <n>          read n bytes from memory\n"                  \
  "memw <addr> <value>     write a word to memory\n"                    \
  "regw <reg> <value>      write a value to a register\n"               \
  "regs                    print all registers\n"                       \
  "disas [addr|symbol]     disassemble around address or current RIP\n" \
  "bt                      print backtrace\n"                           \
  "q                       quit\n"

Debugger::Debugger(const char *program, char *const argv[])
    : mPid(ptSpawn(program, argv)),
      mElf(program),
      mPExited(false),
      mRegs(mPid),
      mBrkPoints() {}

void Debugger::run() {
  std::string line;
  while (true) {
    std::cout << "bugme> " << std::flush;
    if (!std::getline(std::cin, line)) break;
    if (line.empty()) continue;
    handleCommand(line);
  }
}

void Debugger::cnt(int signal) {
  ptContinue(mPid, signal);
  wait();
}

void Debugger::getRegs() {
  if (!mPExited) mRegs.getRegs();
  mRegs.renderRegs();
}

void Debugger::setBP(uint64_t addr) {
  auto [it, inserted] = mBrkPoints.emplace(addr, BreakPoint(mPid, addr));
  if (inserted) {
    it->second.enableBP();
  }
}

void Debugger::setBP(const std::string &symName) {
  if (mElf.isPIE()) {
    auto addr = mElf.getLoadAddress(mPid);
    if (!addr.has_value()) {
      std::cerr << "Binary load address unresolved\n";
      return;
    }
    auto offset = mElf.getSymbolOffset(symName);
    if (!offset.has_value()) {
      std::cerr << "Symbol " << symName << " is undefined.\n";
      return;
    }
    auto [it, inserted] =
        mBrkPoints.emplace(*offset + *addr, BreakPoint(mPid, *addr + *offset));
    if (inserted) it->second.enableBP();
  } else {
    auto addr = mElf.getSymbolOffset(symName);
    if (!addr.has_value()) {
      std::cerr << "Symbol " << symName << " is undefined.\n";
      return;
    }
    auto [it, inserted] = mBrkPoints.emplace(*addr, BreakPoint(mPid, *addr));
    if (inserted) it->second.enableBP();
  }
}

void Debugger::setRegister(const Regs &reg, std::uint64_t value) {
  mRegs.setRegister(reg, value);
}

uint64_t Debugger::getRIP() {
  if (!mPExited) mRegs.getRegs();
  return mRegs.getRegisterValue(Regs::rip);
}

void Debugger::setRIP(uint64_t addr) {
  if (!mPExited) mRegs.getRegs();
  mRegs.setRegister(Regs::rip, addr);
}

void Debugger::wait() {
  int status;
  waitpid(mPid, &status, 0);

  if (WIFEXITED(status)) {
    std::cout << "Tracee exited with code: " << WEXITSTATUS(status) << "\n";
    mPExited = true;
    return;
  }

  if (WIFSTOPPED(status)) {
    mRegs.getRegs();
    if (WSTOPSIG(status) == SIGTRAP) handleTRAP();
  } else if (WIFSIGNALED(status)) {
    int signal = WTERMSIG(status);
    std::cout << "Tracee terminated with signal: " << signal << "\n";
    mPExited = false;
    return;
  }
}

void Debugger::handleTRAP() {
  auto IP = getRIP() - 1;
  if (auto brkPoint = mBrkPoints.find(IP); brkPoint != mBrkPoints.end()) {
    std::cout << "Breakpoint hit at 0x" << std::hex << IP << "\n";
    brkPoint->second.disableBP();
    setRIP(IP);

    ptSingleStep(mPid);
    wait();

    brkPoint->second.enableBP();
  }
}

void Debugger::handleCommand(std::string &line) {
  std::istringstream cmd(line);
  std::vector<std::string> tokens{};
  std::string token;

  while (cmd >> token) {
    tokens.push_back(token);
  }
  if (tokens.empty()) return;

  auto keyW = tokens[0];
  if (keyW == "cnt") {
    cnt(0);
  } else if (keyW == "brk") {
    if (tokens.size() != 2) {
      std::cout << "usage: brk <address or symbol>\n";
      return;
    }
    const std::string &target = tokens[1];
    // Check if it looks like a hex address
    if (target.length() > 2 && target[0] == '0' && target[1] == 'x') {
      try {
        uint64_t addr = std::stoull(target, nullptr, 16);
        setBP(addr);
      } catch (const std::exception &e) {
        std::cerr << "invalid argument for brk. use valid address or symbol\n";
        return;
      }
    } else {
      // Treat as symbol name
      setBP(target);
    }
  } else if (keyW == "regs") {
    if (tokens.size() != 1) {
      std::cerr << "usage: regs\n";
      return;
    }
    getRegs();
  } else if (keyW == "q") {
    if (tokens.size() != 1) {
      std::cerr << "usage: q\n";
      return;
    }
    kill(mPid, SIGTERM);
    exit(0);
  } else if (keyW == "step") {
    if (tokens.size() != 1) {
      std::cerr << "usage: step\n";
      return;
    }
    ptSingleStep(mPid);
    wait();
  } else if (keyW == "regw") {
    if (tokens.size() != 3) {
      std::cerr << "usage: regw <regName> <64-bit hex value>\n";
      return;
    }
    auto reg = mRegs.getRegister(tokens[1]);
    if (!reg.has_value()) {
      std::cerr << tokens[1] << " is not a valid register name\n";
      return;
    }
    try {
      auto value = std::stoull(tokens[2], nullptr, 16);
      setRegister(*reg, static_cast<uint64_t>(value));
    } catch (const std::exception(&e)) {
      std::cerr << "The value must be a valid hex string\n";
      return;
    }
  } else if (keyW == "help") {
    printf(HELP_TXT);
  } else {
    std::cout << "Unknown command " << keyW << "\n";
    return;
  }
}
