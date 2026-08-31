#ifndef DISASSEMBLE_HPP_
#define DISASSEMBLE_HPP_

#include <capstone/capstone.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class Disassembler {
 public:
  Disassembler();
  ~Disassembler();

  Disassembler(const Disassembler&) = delete;
  Disassembler& operator=(const Disassembler&) = delete;
  Disassembler(Disassembler&&) = delete;
  Disassembler& operator=(Disassembler&&) = delete;

  struct Instruction {
    uint64_t addr;            // address of the instruction
    uint16_t size;            // size in bytes occupied by instruction
    std::string instruction;  // ascii representation of instruction
  };

  std::optional<std::vector<Instruction>> disasInstructions(
      const uint8_t* codeSeg, const uint64_t& startAddr, const size_t& num);

 private:
  csh mCSHandle;
};

#endif
