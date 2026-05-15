#include "elfParser.hpp"

#include <elf.h>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

ElfParser::ElfParser(const std::string& executablePath)
    : mPath(executablePath),
      mSymbolTable(),
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

void ElfParser::loadSymbols() {
  if (!mElfHandle)
    if (!loadElf()) return;

  Elf_Scn* section = NULL;
  GElf_Shdr sectionHdr;

  while ((section = elf_nextscn(mElfHandle, section)) != NULL) {
    gelf_getshdr(section, &sectionHdr);

    // static symbol table
    if (sectionHdr.sh_type == SHT_SYMTAB) {
      Elf_Data* data = elf_getdata(section, NULL);
      uint32_t num = sectionHdr.sh_size / sectionHdr.sh_entsize;

      for (auto idx = 0; idx < num; idx++) {
        GElf_Sym symEntry;
        if (!gelf_getsym(data, idx, &symEntry)) continue;

        if (symEntry.st_shndx == SHN_UNDEF || symEntry.st_value == 0) continue;

        std::string name =
            elf_strptr(mElfHandle, sectionHdr.sh_link, symEntry.st_name) ?: "";
        if (!name.empty()) mSymbolTable.emplace(name, symEntry.st_value);
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
