#include "debugger.hpp"

#include <sys/types.h>
#include <sys/wait.h>

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <ios>
#include <iostream>
#include <optional>
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
  "memr <addr> <n>          read n bytes from memory\n"                 \
  "memw <addr> <value>     write a word to memory\n"                    \
  "regw <reg> <value>      write a value to a register\n"               \
  "regs                    print all registers\n"                       \
  "disas [addr|symbol]     disassemble around address or current RIP\n" \
  "bt                      print backtrace\n"                           \
  "q                       quit\n"

Debugger::Debugger(const char* program, char* const argv[])
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

void Debugger::setRegister(const Regs& reg, std::uint64_t value) {
  mRegs.setRegister(reg, value);
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
    mPExited = true;
    return;
  }
}

std::optional<Debugger::MemRegion> Debugger::getRegion(const uint64_t& addr) {
  auto procPath = "/proc/" + std::to_string(mPid) + "/maps";
  std::ifstream procFile(procPath);
  std::string line;

  while (std::getline(procFile, line)) {
    std::string addrSpace, perms;
    std::istringstream strStream(line);

    strStream >> addrSpace >> perms;
    uint64_t start, end;
    sscanf(addrSpace.c_str(), "%lx-%lx", &start, &end);
    if (start <= addr && addr <= end) {
      return MemRegion(
          {start, end, perms[0] == 'r', perms[1] == 'w', perms[2] == 'x'});
    }
  }

  return std::nullopt;
}

void Debugger::memWrite(std::uint64_t addr, std::uint64_t value) {
  auto region = getRegion(addr);
  if (!region.has_value()) {
    std::cerr << "0x" << std::hex << addr << " is not mapped into memory\n";
    return;
  }
  if (!region->w) {
    std::cerr << "0x" << std::hex << addr << " is not writeable\n";
    return;
  }
  if (addr + 7 > region->endAddr) {
    std::cerr << "can not write a 64-bit word to 0x" << std::hex << addr
              << "\n";
    return;
  }
  ptWriteMem(mPid, addr, value);
}

void Debugger::memRead(std::uint64_t addr, std::uint16_t n) {
  auto region = getRegion(addr);
  if (!region.has_value()) {
    std::cerr << "0x" << std::hex << addr << " is not mapped into memory\n";
    return;
  }
  if (!region->r) {
    std::cerr << "0x" << std::hex << addr << " is not readable\n";
    return;
  }
  if (addr + n > region->endAddr) {
    std::cerr << n << " bytes from 0x" << std::hex << addr
              << " is not readable\n";
    std::cout << "only " << (n = region->endAddr - addr)
              << " bytes can be read\n";
  }
  for (auto i = 0; i < n; i += 16) {
    auto left = n - i;  // number of bytes left to read
    std::cout << "0x" << std::hex << addr + i << ": ";
    std::string rep{};
    long word = ptReadMem(mPid, addr + i);
    for (int j = 7; left > 0 && j >= 0;
         j--) {  // correction for little-endian order
      uint8_t byte = (word >> (j * 8)) & 0xff;
      printf("%02x ", byte);
      rep.push_back(byte > 31 && byte < 127 ? byte : '.');
      left--;
    }
    std::cout << "    ";

    word = ptReadMem(mPid, addr + i + 8);
    for (int j = 7; left > 0 && j >= 0;
         j--) {  // correction for little-endian order
      uint8_t byte = (word >> (j * 8)) & 0xff;
      printf("%02x ", byte);
      rep.push_back(byte > 31 && byte < 127 ? byte : '.');
      left--;
    }

    auto written = n - i - left;
    auto charsWritten = written * 3 + (written > 8 ? 4 : 0);
    for (int j = 0; j < 56 - charsWritten; j++) std::cout << " ";
    std::cout << rep << "\n";
  }
}

void Debugger::handleTRAP() {
  auto IP = mRegs.getRegisterValue(Regs::rip) - 1;
  if (auto brkPoint = mBrkPoints.find(IP); brkPoint != mBrkPoints.end()) {
    std::cout << "Breakpoint hit at 0x" << std::hex << IP << "\n";
    brkPoint->second.disableBP();
    mRegs.setRegister(Regs::rip, IP);

    ptSingleStep(mPid);
    wait();

    brkPoint->second.enableBP();
  }
}

uint64_t Debugger::parseAddr(const std::string& expr) {
  size_t plusPos = expr.find('+');
  size_t minusPos = expr.find('-');

  if (plusPos != std::string::npos && minusPos != std::string::npos)
    throw std::invalid_argument("multiple operators in expression: " + expr);

  size_t opPos = std::string::npos;
  char op = 0;

  if (plusPos != std::string::npos) {
    opPos = plusPos;
    op = '+';
  } else if (minusPos != std::string::npos) {
    opPos = minusPos;
    op = '-';
  }

  std::string addrPart =
      (opPos != std::string::npos) ? expr.substr(0, opPos) : expr;

  if (addrPart.empty())
    throw std::invalid_argument("missing address in expression: " + expr);

  uint64_t base = 0;

  if (addrPart.size() > 2 && addrPart[0] == '0' && addrPart[1] == 'x') {
    try {
      base = std::stoull(addrPart, nullptr, 16);
    } catch (...) {
      throw std::invalid_argument("invalid hex address: " + addrPart);
    }
  } else {
    if (mElf.isPIE()) {
      auto loadAddr = mElf.getLoadAddress(mPid);
      if (!loadAddr.has_value())
        throw std::runtime_error(
            "could not resolve load address for PIE binary");
      auto symOffset = mElf.getSymbolOffset(addrPart);
      if (!symOffset.has_value())
        throw std::invalid_argument("unknown symbol: " + addrPart);
      base = *loadAddr + *symOffset;
    } else {
      auto symAddr = mElf.getSymbolOffset(addrPart);
      if (!symAddr.has_value())
        throw std::invalid_argument("unknown symbol: " + addrPart);
      base = *symAddr;
    }
  }

  if (opPos != std::string::npos) {
    std::string offsetPart = expr.substr(opPos + 1);

    if (offsetPart.empty())
      throw std::invalid_argument("missing offset after '" +
                                  std::string(1, op) + "'");

    uint64_t offset = 0;
    try {
      offset = std::stoull(offsetPart, nullptr, 10);
    } catch (...) {
      throw std::invalid_argument("offset must be a decimal number, got: " +
                                  offsetPart);
    }

    if (op == '+') {
      base += offset;
    } else {
      if (offset > base)
        throw std::underflow_error("subtraction would underflow: " + expr);
      base -= offset;
    }
  }

  return base;
}

void Debugger::handleCommand(std::string& line) {
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
    try {
      auto addr = parseAddr(tokens[1]);
      setBP(addr);
    } catch (const std::exception& e) {
      std::cerr << "brk: " << e.what() << "\n";
      return;
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
  } else if (keyW == "memw") {
    if (tokens.size() != 3) {
      std::cerr << "usage: memw <addr> <64-bit hex val>\n";
      return;
    }
    std::uint64_t addr = 0;
    try {
      addr = parseAddr(tokens[1]);
    } catch (const std::exception& e) {
      std::cerr << e.what() << '\n';
      return;
    }

    try {
      auto val = std::stoull(tokens[2], NULL, 16);
      memWrite(addr, val);
    } catch (const std::exception& e) {
      std::cerr << tokens[2] << " is not a valid hex string\n";
      return;
    }
  } else if (keyW == "memr") {
    if (tokens.size() != 2 && tokens.size() != 3) {
      std::cerr << "usage: memr <addr> <number of bytes> (optional)\n";
      return;
    }
    std::uint64_t addr = 0;
    try {
      addr = parseAddr(tokens[1]);
    } catch (const std::exception& e) {
      std::cerr << e.what() << '\n';
      return;
    }
    if (tokens.size() == 3) {
      try {
        auto val = std::stoi(tokens[2]);
        memRead(addr, val);
      } catch (const std::exception& e) {
        std::cerr << "invalid byte count " << tokens[2] << "\n";
        return;
      }
    } else
      memRead(addr);
  } else if (keyW == "help") {
    printf(HELP_TXT);
  } else {
    std::cout << "Unknown command " << keyW << "\n";
    return;
  }
}
