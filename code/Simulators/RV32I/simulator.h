#pragma once

#include "Cpu/cpu.h"

#include <Expressions/expression.h>
#include <vector>

namespace Simulators
{
namespace RV32I
{

class Simulator
{
public:
   Simulator();

   void run(Assemblers::RV32I::ExecutableProgram& executable);

   std::uint32_t getNumberOfRanInstructions();
   std::uint32_t getNumberOfBitFlips();
   std::int32_t getRegister(const std::string& name) { return m_cpu.getRegister(name); }
   std::vector<std::string> getRegisterNames() { return m_cpu.getRegisterNames(); }

private:
   CPU m_cpu;
};

}
}
