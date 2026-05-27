#include "disassembler.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "capstone.h"

Disassembler::Disassembler() : mCSHandle(0) {
  if (cs_open(CS_ARCH_X86, CS_MODE_64, &mCSHandle) != CS_ERR_OK)
    throw std::runtime_error("Capstone initialisation failed");
}

Disassembler::~Disassembler() { cs_close(&mCSHandle); }

std::optional<std::vector<Disassembler::Instruction>>
Disassembler::disasInstructions(const uint8_t* codeSeg,
                                const uint64_t& startAddr, const size_t& size) {
  cs_insn* instructions;
  auto disasNum =
      cs_disasm(mCSHandle, codeSeg, size, startAddr, 0, &instructions);

  std::vector<Instruction> disasCode;
  disasCode.reserve(disasNum);

  for (auto i = 0; i < disasNum; i++) {
    Instruction in;
    in.instruction = std::string(instructions[i].mnemonic);
    if (instructions[i].op_str[0] != 0)
      in.instruction += std::string(" ") + std::string(instructions[i].op_str);

    in.size = instructions[i].size;
    in.addr = instructions[i].address;

    disasCode.push_back(in);
  }

  if (instructions) cs_free(instructions, disasNum);

  if (disasNum > 0)
    return disasCode;
  else
    return std::nullopt;
}
