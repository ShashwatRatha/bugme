#include "elfParser.hpp"

#include <elf.h>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

ElfParser::ElfParser(const std::string& executablePath)
    : mPath(executablePath),
      mSymbolTable(),
      mAddrMap(),
      mIsPIE(false),
      mElfHandle(nullptr),
      mFD(-1) {
  determineIfPIE();
}

ElfParser::~ElfParser() {
  if (mElfHandle) {
    elf_end(mElfHandle);
    mElfHandle = nullptr;
  }

  if (mFD != -1) {
    close(mFD);
    mFD = -1;
  }
}

std::optional<uint64_t> ElfParser::getSymbolOffset(const std::string& name) {
  if (mSymbolTable.empty()) loadSymbols();

  if (auto it = mSymbolTable.find(name); it != mSymbolTable.end())
    return it->second;
  return std::nullopt;
}

std::optional<uint64_t> ElfParser::getLoadAddress(pid_t pid) {
  auto procPath = "/proc/" + std::to_string(pid) + "/maps";
  std::ifstream procFile(procPath);
  std::string line;

  while (std::getline(procFile, line)) {
    if (line.find(std::filesystem::absolute(mPath).string()) !=
        std::string::npos) {
      std::string addrSpace, perms, offset;
      std::istringstream strStream(line);

      strStream >> addrSpace >> perms >> offset;
      if (std::stoull(offset, nullptr, 16) == 0 &&
          (perms == "r--p" || perms == "r-xp")) {
        return std::stoull(addrSpace.substr(0, addrSpace.find('-')), nullptr,
                           16);
      }
    }
  }

  return std::nullopt;
}

bool ElfParser::isPIE() const { return mIsPIE; }

bool ElfParser::loadElf() {
  if (mElfHandle) return true;

  if (elf_version(EV_CURRENT) == EV_NONE) return false;

  int fd = open(mPath.c_str(), O_RDONLY);
  if (fd == -1) {
    std::perror("open");
    return false;
  }

  mFD = fd;
  mElfHandle = elf_begin(mFD, ELF_C_READ, NULL);
  return true;
}

std::optional<std::string> ElfParser::getSymbolName(const std::uint64_t& addr) {
  if (mAddrMap.empty()) loadSymbols();
  auto upAddr = mAddrMap.upper_bound(addr);
  if (upAddr == mAddrMap.begin()) return std::nullopt;
  upAddr--;

  auto [keyAddr, symbol] = *upAddr;

  if (symbol.second > 0 && addr > keyAddr + symbol.second) return std::nullopt;
  return symbol.first;
}

std::optional<std::size_t> ElfParser::getSymbolSize(const std::uint64_t& addr) {
  if (mAddrMap.empty()) loadSymbols();
  auto upAddr = mAddrMap.upper_bound(addr);
  if (upAddr == mAddrMap.begin()) return std::nullopt;
  upAddr--;

  auto [keyAddr, symbol] = *upAddr;

  if (symbol.second > 0 && addr > keyAddr + symbol.second) return std::nullopt;
  return symbol.second;
}

void ElfParser::loadSymbols() {
  if (!mElfHandle)
    if (!loadElf()) return;

  Elf_Scn* section = NULL;
  GElf_Shdr sectionHdr;

  while ((section = elf_nextscn(mElfHandle, section)) != NULL) {
    gelf_getshdr(section, &sectionHdr);

    // static symbol table
    if (sectionHdr.sh_type == SHT_SYMTAB || sectionHdr.sh_type == SHT_DYNSYM) {
      Elf_Data* data = elf_getdata(section, NULL);
      uint32_t num = sectionHdr.sh_size / sectionHdr.sh_entsize;

      for (auto idx = 0; idx < num; idx++) {
        GElf_Sym symEntry;
        if (!gelf_getsym(data, idx, &symEntry)) continue;

        if (symEntry.st_shndx == SHN_UNDEF || symEntry.st_value == 0) continue;

        std::string name =
            elf_strptr(mElfHandle, sectionHdr.sh_link, symEntry.st_name) ?: "";
        if (!name.empty()) mSymbolTable.emplace(name, symEntry.st_value);

        if (GELF_ST_TYPE(symEntry.st_info) == STT_FUNC) {
          mAddrMap.emplace(
              static_cast<uint64_t>(symEntry.st_value),
              std::make_pair(name, static_cast<uint64_t>(symEntry.st_size)));
        }
      }
    }
  }
}

void ElfParser::determineIfPIE() {
  if (!mElfHandle)
    if (!loadElf()) return;

  GElf_Ehdr elfHdr;
  if (gelf_getehdr(mElfHandle, &elfHdr)) {
    mIsPIE = (elfHdr.e_type == ET_DYN);
  }
}
