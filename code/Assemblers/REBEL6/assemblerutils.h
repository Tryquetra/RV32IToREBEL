#pragma once

#include <cstdint>
#include <string>

namespace Assemblers
{
namespace REBEL6
{

class ExecutableProgram;

class AssemblerUtils
{
 public:
   enum class InstructionType {BRANCH, IMMEDIATE, JUMP, JUMP_REGISTER, LOAD, LOAD_IMMEDIATE, REGISTER, STORE, SYSTEM, UNDEFINED};

   AssemblerUtils() = delete;

   static InstructionType getInstructionType(const std::string& instructionName);
   // Width of the immediate field in trits for this instruction type, 0 when it has none.
   static std::uint8_t immediateWidthTrits(InstructionType type);
   static void generateAssemblyFileForMRCS(const ExecutableProgram& program, const std::string& fileName);

};

}
}
