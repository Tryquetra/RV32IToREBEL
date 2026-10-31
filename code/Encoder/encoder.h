#pragma once

#include <Expressions/instruction.h>
#include <Assemblers/REBEL6/executableprogram.h>

#include <cstdint>
#include <string>

namespace Encoder
{
// Encodes one instruction whose operands are already resolved, into the packed 32-trit form.
bool encode(const Expressions::Instruction& instr, std::uint64_t& word, std::string& error);

// Writes the R6IM container used by rebel6_asm: magic, counts, base, entry, words, data.
bool writeImage(const std::string& path, const Assemblers::REBEL6::ExecutableProgram& program,
                std::int64_t entry, std::string& error);
}
