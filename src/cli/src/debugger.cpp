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
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "brkPoint.hpp"
#include "ptraceWrappers.h"
#include "registers.hpp"

#define HELP_TXT                                                               \
  "cnt                     continue execution\n"                               \
  "step                    single-step one instruction\n"                      \
  "brk <addr|symbol>       set a breakpoint\n"                                 \
  "memr <addr> [n]         read n (default = 64) bytes from memory\n"          \
  "memw <addr> <value>     write a word to memory\n"                           \
  "regw <reg> <value>      write a value to a register\n"                      \
  "regs                    print all registers\n"                              \
  "disas [addr|symbol] [n] disassemble around address or current RIP to show " \
  "n (default = 16) instructions\n"                                            \
  "bt                      print backtrace\n"                                  \
  "help                    show this help\n"                                   \
  "q                       quit\n"

using CommandArg = const std::vector<std::string>&;

Debugger::Debugger(const char* program, char* const argv[])
    : mPid(ptSpawn(program, argv)),
      mElf(program),
      mPExited(false),
      mRegs(mPid),
      // mDisas(),
      mAddrInsn(),
      // mDisasInstructions(),
      mCommands(),
      mBrkPoints() {
  loadCommands();
  loadDisassembly();
}

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
    std::cout << "\n### Tracee terminated with signal: " << signal << "\n";
    mPExited = true;
    return;
  }
}

void Debugger::loadDisassembly() {
  auto ts = mElf.loadTextSection();
  if (!ts.has_value()) {
    std::cerr << "could not load text section\n";
    return;
  }

  auto disasInsns = mDisas.disasInstructions(ts->bytes.data(), ts->startAddr,
                                             ts->bytes.size());
  if (!disasInsns.has_value()) {
    std::cerr << "could not disassemble code segment";
    return;
  }

  mDisasInstructions = *disasInsns;

  for (auto i = 0; i < mDisasInstructions.size(); i++)
    mAddrInsn.emplace(mDisasInstructions[i].addr, i);
}

void Debugger::renderDisassembly(const uint64_t& addr, const size_t& num) {
  if (mPExited) {
    std::cerr << "tracee has exited\n";
    return;
  }
  mRegs.getRegs();
  uint64_t lookupAddr = addr;
  if (mElf.isPIE()) {
    auto loadAddr = mElf.getLoadAddress(mPid);
    if (!loadAddr.has_value()) {
      std::cerr << "couldnt resolve load address\n";
      return;
    }
    if (*loadAddr < lookupAddr) lookupAddr -= *loadAddr;
  }
  auto offset = addr - lookupAddr;
  auto rip = mRegs.getRegisterValue(Regs::rip);

  if (auto it = mAddrInsn.find(lookupAddr); it != mAddrInsn.end()) {
    auto ndx = it->second;
    auto ptr = 0;
    for (auto idx = ndx;
         idx < std::min(num + ndx + 1, mDisasInstructions.size()); idx++) {
      bool isRIP = mDisasInstructions[idx].addr == (rip - offset);
      bool isBRK = mBrkPoints.count(mDisasInstructions[idx].addr + offset) > 0;
      printf("%s%s0x%016lx <+%d>: %s\n", isBRK ? " " : "  ",
             isRIP ? "▶ " : "  ", mDisasInstructions[idx].addr, ptr,
             mDisasInstructions[idx].instruction.c_str());
      ptr += mDisasInstructions[idx].size;
    }
  } else {
    std::cerr << "lookup at 0x" << std::hex << lookupAddr << " is invalid.\n";
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

void Debugger::memRead(std::uint64_t addr, std::size_t n) {
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

    int status;
    waitpid(mPid, &status, 0);

    brkPoint->second.enableBP();
  }
}

void Debugger::loadCommands() {
  mCommands = {
      {"q",
       [&](CommandArg tokens) {
         if (tokens.size() != 1) {
           std::cerr << "usage: q\n";
           return;
         }
         if (!mPExited) {
           std::cout << "### The process (PID: " << mPid
                     << ") is running. Do you want to close the debugger?\n"
                        "(type y/Y to exit) ";
           std::string response;
           std::getline(std::cin, response);
           if (response.back() == '\n') response.pop_back();
           if (response != "Y" && response != "y") return;
         }
         kill(mPid, SIGTERM);
         exit(0);
       }},
      {"brk",
       [&](CommandArg tokens) {
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
       }},
      {"cnt", [&](CommandArg tokens) { cnt(); }},
      {"regs",
       [&](CommandArg tokens) {
         if (tokens.size() != 1) {
           std::cerr << "usage: regs\n";
           return;
         }
         getRegs();
       }},
      {"regw",
       [&](CommandArg tokens) {
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
       }},
      {"memr",
       [&](CommandArg tokens) {
         if (tokens.size() != 2 && tokens.size() != 3) {
           std::cerr << "usage: memr <addr> [n]\n";
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
         } else {
           memRead(addr);
         }
       }},
      {"memw",
       [&](CommandArg tokens) {
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
       }},
      {"bt",
       [&](CommandArg tokens) {
         if (tokens.size() != 1) {
           std::cerr << "usage: bt\n";
           return;
         }
         backTrace();
       }},
      {"step",
       [&](CommandArg tokens) {
         if (tokens.size() != 1) {
           std::cerr << "usage: step\n";
           return;
         }
         ptSingleStep(mPid);
         wait();
       }},
      {"help", [&](CommandArg tokens) { printf(HELP_TXT); }},
      {"disas", [&](CommandArg tokens) {
         if (tokens.size() != 2 && tokens.size() != 3) {
           std::cerr << "usage: disas <hex addr|symbol> [n]\n";
           return;
         }
         uint64_t addr = 0;
         try {
           addr = parseAddr(tokens[1]);
         } catch (const std::exception& e) {
           std::cerr << e.what() << "\n";
           return;
         }
         auto region = getRegion(addr);
         if (!region.has_value()) {
           std::cerr << "0x" << std::hex << addr
                     << " is not mapped into memory\n";
           return;
         }
         if (!region->r) {
           std::cerr << "0x" << std::hex << addr << " is not readable\n";
           return;
         }
         if (tokens.size() == 3) {
           try {
             auto num = std::stoul(tokens[2]);
             renderDisassembly(addr, num);
           } catch (const std::exception& e) {
             std::cerr << e.what() << "\n";
             return;
           }
         } else {
           auto name = mElf.getSymbolName(addr);
           if (name.has_value()) {
             uint64_t lookupAddr = addr;
             if (mElf.isPIE()) {
               auto loadAddr = mElf.getLoadAddress(mPid);
               if (!loadAddr.has_value()) {
                 std::cerr << "couldnt resolve load address\n";
                 return;
               }
               if (*loadAddr < lookupAddr) lookupAddr -= *loadAddr;
             }
             auto offset = addr - lookupAddr;
             auto idx = mAddrInsn.find(lookupAddr)->second, i = idx;
             while (i < mDisasInstructions.size() &&
                    mDisasInstructions[i++].instruction != "ret");
             std::cout << *name << ":\n";
             renderDisassembly(addr, i - 1 - idx);
           } else {
             renderDisassembly(addr);
           }
         }
       }}};
}

void Debugger::backTrace() {
  if (mPExited) {
    std::cerr << "tracee has exited\n";
    return;
  }
  mRegs.getRegs();
  auto rip = mRegs.getRegisterValue(Regs::rip);
  auto rbp = mRegs.getRegisterValue(Regs::rbp);
  auto frameNumber = 0;

  while (true) {
    if (rbp == 0) break;

    auto lookupAddr = rip;
    if (mElf.isPIE()) {
      auto loadAddr = mElf.getLoadAddress(mPid);
      if (loadAddr.has_value() && lookupAddr >= *loadAddr)
        lookupAddr -= *loadAddr;
    }
    auto name = mElf.getSymbolName(lookupAddr);

    printf("%d: %016lx in %s\n", frameNumber++, rip,
           (name.has_value() ? name->c_str() : "???"));

    auto rbpParent = ptReadMem(mPid, rbp);
    auto ripParent = ptReadMem(mPid, rbp + 8);
    if (rbpParent == -1 || ripParent == -1) break;

    auto newRbp = static_cast<uint64_t>(rbpParent);
    auto newRip = static_cast<uint64_t>(ripParent);
    if (newRbp == 0) break;

    rbp = newRbp, rip = newRip;
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

  if (auto it = mCommands.find(tokens[0]); it != mCommands.end()) {
    it->second(tokens);
  } else {
    std::cerr << "unknown command: " << tokens[0] << "\n";
    return;
  }
}
