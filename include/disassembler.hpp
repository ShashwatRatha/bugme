#ifndef DISASSEMBLE_HPP_
#define DISASSEMBLE_HPP_

#include <capstone/capstone.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Thin layer wrapped around the Capstone disassembler engine for x86_64
 * architecture decoding.
 */
class Disassembler {
 public:
  /**
   * @brief Initializes Capstone library handle states for x86_64 64-bit
   * execution decoding mode.
   */
  Disassembler();

  /**
   * @brief Frees allocated internal Capstone context handles safely.
   */
  ~Disassembler();

  Disassembler(const Disassembler&) = delete;
  Disassembler& operator=(const Disassembler&) = delete;
  Disassembler(Disassembler&&) = delete;
  Disassembler& operator=(Disassembler&&) = delete;

  /**
   * @brief Decoded representation of a single machine instruction.
   */
  struct Instruction {
    uint64_t addr;  ///< Target instruction pointer memory address.
    uint16_t
        size;  ///< Total size footprint in bytes occupied by this operation.
    std::string instruction;  ///< Combined textual assembly representation
                              ///< (mnemonic + operands).
  };

  /**
   * @brief Deconstructs raw binary byte buffers into human-readable assembly
   * lines.
   * * @param codeSeg Pointer to the raw buffer array of machine opcode
   * instructions.
   * @param startAddr Virtual memory address baseline representing the starting
   * point offset of the code.
   * @param num Size bounds footprint limit of the passed binary buffer byte
   * track.
   * @return A vector filled with decoded Instructions on success, std::nullopt
   * if the decoding engine fails.
   */
  std::optional<std::vector<Instruction>> disasInstructions(
      const uint8_t* codeSeg, const uint64_t& startAddr, const size_t& num);

 private:
  csh mCSHandle;  ///< Capstone engine handle instance.
};

#endif
