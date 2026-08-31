#ifndef ELF_PARSER_HPP_
#define ELF_PARSER_HPP_

#include <gelf.h>
#include <libelf.h>
#include <sys/types.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Simple wrapper around libelf to handle symbol resolution
 * and ELF metadata.
 */
class ElfParser {
 public:
  ElfParser() = delete;
  explicit ElfParser(const std::string& executablePath);
  ~ElfParser();

  ElfParser(const ElfParser&) = delete;
  ElfParser(ElfParser&&) = delete;
  ElfParser& operator=(const ElfParser&) = delete;
  ElfParser& operator=(ElfParser&&) = delete;

  /**
   * @brief Looks up a symbol name in the .symtab (and optionally .dynsym).
   * @param name The function or variable name (e.g., "main").
   * @return The offset/address if found, otherwise std::nullopt.
   */
  std::optional<uint64_t> getSymbolOffset(const std::string& name);

  /**
   * @brief Parses the /proc/<pid>/maps to find where the binary is loaded.
   * Essential for PIE support.
   * @param pid The process ID of the tracee.
   * @return The base address of the executable in memory.
   */
  std::optional<uint64_t> getLoadAddress(pid_t pid);

  /**
   * @brief Checks if the binary is a Position Independent Executable (PIE).
   */
  struct TextSection {
    uint64_t startAddr;
    std::vector<uint8_t> bytes;
  };
  bool isPIE() const;
  std::optional<std::string> getSymbolName(const std::uint64_t& addr);
  std::optional<std::size_t> getSymbolSize(const std::uint64_t& addr);
  std::optional<TextSection> loadTextSection();

 private:
  std::string mPath;
  Elf* mElfHandle = nullptr;
  int mFD = -1;
  bool mIsPIE = false;
  std::unordered_map<std::string, uint64_t> mSymbolTable;
  std::map<std::uint64_t, std::pair<std::string, std::size_t>> mAddrMap;

  bool loadElf();
  void loadSymbols();
  void determineIfPIE();
};

#endif
