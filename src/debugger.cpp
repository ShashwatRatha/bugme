#include "debugger.hpp"

#include <sys/types.h>
#include <sys/wait.h>

#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "brkPoint.hpp"
#include "ptraceWrappers.h"
#include "registers.hpp"

Debugger::Debugger(const char *program, char *const argv[])
    : mPid(ptSpawn(program, argv)), mRegs(mPid), mBrkPoints() {}

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
  mRegs.getRegs();
  mRegs.renderRegs();
}

void Debugger::setBP(uint64_t addr) {
  auto [it, inserted] = mBrkPoints.emplace(addr, BreakPoint(mPid, addr));
  if (inserted) {
    it->second.enableBP();
  }
}

void Debugger::setRegister(Regs reg, uint64_t value) {
  mRegs.setRegister(reg, value);
}

uint64_t Debugger::getRIP() {
  mRegs.getRegs();
  return mRegs.getRegister(Regs::rip);
}

void Debugger::setRIP(uint64_t addr) {
  mRegs.getRegs();
  mRegs.setRegister(Regs::rip, addr);
}

void Debugger::wait() {
  int status;
  waitpid(mPid, &status, 0);

  if (WIFEXITED(status)) {
    std::cout << "Tracee exited with code: " << WEXITSTATUS(status) << "\n";
    exit(0);
  }

  if (WIFSTOPPED(status)) {
    if (WSTOPSIG(status) == SIGTRAP) handleTRAP();
  } else if (WIFSIGNALED(status)) {
    int signal = WTERMSIG(status);
    std::cout << "Tracee terminated with signal: " << signal << "\n";
    exit(0);
  }
}

void Debugger::handleTRAP() {
  auto IP = getRIP() - 1;
  if (auto brkPoint = mBrkPoints.find(IP); brkPoint != mBrkPoints.end()) {
    std::cout << "Breakpoint hit at 0x" << std::hex << IP << "\n";
    brkPoint->second.disableBP();
    setRIP(IP);
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
    setBP(std::stoull(tokens[1], nullptr, 16));
  } else if (keyW == "regs") {
    getRegs();
  } else if (keyW == "q") {
    kill(mPid, SIGTERM);
    exit(0);
  } else {
    std::cout << "Unknown command " << keyW << "\n";
    return;
  }
}
