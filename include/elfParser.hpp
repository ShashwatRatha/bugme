#ifndef ELF_PARSER_HPP_
#define ELF_PARSER_HPP_

#include <gelf.h>
#include <sys/types.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Utility module encapsulating libelf actions to extract system symbols
 * and section data.
 */
class ElfParser {
 public:
  /**
   * @brief Prepares metadata parser layers and opens standard file descriptors
   * tracking the target application.
   * @param executablePath Path tracking target executable target program source
   * file.
   */
  explicit ElfParser(const std::string& executablePath);

  /**
   * @brief Safely unlinks active libelf contexts and closes open file handles.
   */
  ~ElfParser();

  ElfParser(const ElfParser&) = delete;
  ElfParser& operator=(const ElfParser&) = delete;

  /**
   * @brief Looks up a text symbol name string inside cached lookup mappings to
   * locate offsets.
   * @param name Name identifier string targeting functions/variables (e.g.,
   * "main").
   * @return 64-bit virtual address/offset offset boundary if found,
   * std::nullopt otherwise.
   */
  std::optional<uint64_t> getSymbolOffset(const std::string& name);

  /**
   * @brief Parses out tracee memory map segments to pin the active relative
   * base load offset.
   * * Analyzes file streams targeting `/proc/<pid>/maps` to find the first
   * executable code segment, establishing an absolute offset anchor necessary
   * for debugging Position Independent Executables (PIE).
   * * @param pid Process ID footprint targeting tracee application processes.
   * @return The verified active virtual runtime load address base if parsed,
   * std::nullopt otherwise.
   */
  std::optional<uint64_t> getLoadAddress(pid_t pid);

  /**
   * @brief Represents raw execution data pulled directly from the target's
   * `.text` boundaries.
   */
  struct TextSection {
    uint64_t startAddr;  ///< Baseline start file offset of code segment section
                         ///< boundaries.
    std::vector<uint8_t> bytes;  ///< Array block container maintaining raw
                                 ///< copied binary contents.
  };

  /**
   * @brief Checks if the binary metadata header confirms PIE configuration.
   * @return true if the execution targets relative offsets (ET_DYN), false if
   * compiled to rigid bases (ET_EXEC).
   */
  bool isPIE() const;

  /**
   * @brief Looks up an address to find its matching string symbol name
   * identifier.
   * @param addr Absolute execution memory virtual pointer.
   * @return Found symbol name string component, std::nullopt otherwise.
   */
  std::optional<std::string> getSymbolName(const std::uint64_t& addr);

  /**
   * @brief Looks up an address to determine the size footprint of its enclosing
   * symbol context.
   * @param addr Absolute execution memory virtual pointer.
   * @return Size of symbol block footprint tracking total allocation
   * boundaries, std::nullopt otherwise.
   */
  std::optional<std::size_t> getSymbolSize(const std::uint64_t& addr);

  /**
   * @brief Isolates and reads out entire binary streams tracking out the
   * application text segments.
   * @return A TextSection struct enclosing binary arrays and offset
   * configurations, std::nullopt on error.
   */
  std::optional<TextSection> loadTextSection();

 private:
  std::string
      mPath;  ///< Local system workspace storage path targeting program files.
  Elf* mElfHandle =
      nullptr;  ///< Pointer mapping active Libelf low level control structures.
  int mFD = -1;  ///< Tracked native system file descriptor handling open files.
  bool mIsPIE =
      false;  ///< Cached flag storing Position Independent Executable status.

  /// Quick-lookup table mapping string name keys to virtual address markers.
  std::unordered_map<std::string, uint64_t> mSymbolTable;

  /// Address map sorting addresses sequentially to extract structural
  /// identities and lengths.
  std::map<std::uint64_t, std::pair<std::string, std::size_t>> mAddrMap;

  /**
   * @brief Handles system level descriptor linkage setup and runs verification
   * validation on version headers.
   * @return true if initialization completes successfully, false if validations
   * throw errors.
   */
  bool loadElf();

  /**
   * @brief Walks through text section blocks (`.symtab` / `.dynsym`) to cache
   * functional names, sizes, and addresses.
   */
  void loadSymbols();

  /**
   * @brief Interrogates high-level structural identity fields inside ELF
   * identity descriptors to verify compilation layouts.
   */
  void determineIfPIE();
};

#endif
