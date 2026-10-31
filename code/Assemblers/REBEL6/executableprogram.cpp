#include "executableprogram.h"

#include <Translators/rv32itorebel6.h>
#include <Assemblers/REBEL6/assemblerutils.h>
#include <Expressions/instruction.h>
#include <logger.h>
#include <errors.h>

#include <iostream>
#include <math.h>

namespace
{
// Initial allocation. The heap grows on demand up to the memory limit.
const std::uint32_t INITIAL_PROGRAM_SIZE_TRYTES = POW3[13] + 1; // About 1.6m trytes, add +1 so it becomes 4 tryte aligned and works with RV32I pointer arithmetic
// Matches the 8 MiB data memory of the Setun-Doom REBEL-6 core.
const std::uint32_t DEFAULT_MEMORY_LIMIT_TRYTES = 8u * 1024u * 1024u;
}

namespace Assemblers
{
namespace REBEL6
{

ExecutableProgram::ExecutableProgram()
    : m_instructionsSize(0)
    , m_heapSizeTrytes(0)
    , m_memoryLimitTrytes(DEFAULT_MEMORY_LIMIT_TRYTES)
{
   m_memory.resize(INITIAL_PROGRAM_SIZE_TRYTES, 0);
}

ExecutableProgram::~ExecutableProgram()
{
   for(Expressions::Instruction* instruction: m_instructions)
   {
      delete instruction;
   }
}

void ExecutableProgram::addInstruction(Expressions::Instruction* instruction, std::uint8_t instructionSize)
{
   instruction->setType(static_cast<std::uint8_t>(AssemblerUtils::getInstructionType(instruction->getInstructionName())));
   m_instructions.push_back(instruction);
   m_instructionsSize += instructionSize;
}

void ExecutableProgram::addToHeap(const Tint& value, std::uint8_t numTrytes)
{
   if((numTrytes > 4))
   {
      throw RebelError("unsupported store size " + std::to_string(numTrytes));
   }

   doStoreToHeap(m_heapSizeTrytes, value, numTrytes);
   m_heapSizeTrytes += numTrytes;
}

void ExecutableProgram::reserveHeap(std::uint32_t numTrytes)
{
   ensureHeapCapacity(m_heapSizeTrytes + numTrytes);
   m_heapSizeTrytes += numTrytes;
}

bool ExecutableProgram::addSymbol(const std::string& symbolName, std::int32_t address)
{
   return m_symbolTable.insert({symbolName, address}).second;
}

Expressions::Instruction* ExecutableProgram::loadInstruction(std::int32_t programCounter, std::uint8_t& instructionSize) const
{
   if((programCounter < 0) || (static_cast<std::uint32_t>(programCounter) >= m_instructions.size()))
   {
      return nullptr;
   }
   instructionSize = 1; // Every REBEL-6 instruction occupies one slot.
   return m_instructions[static_cast<std::uint32_t>(programCounter)];
}

void ExecutableProgram::ensureHeapCapacity(std::uint32_t endAddress)
{
   if(endAddress <= m_memory.size())
   {
      return;
   }
   if(endAddress > m_memoryLimitTrytes)
   {
      throw std::length_error(
         "Program needs " + std::to_string(endAddress) + " heap trytes, memory limit is "
         + std::to_string(m_memoryLimitTrytes));
   }

   std::uint64_t grown = std::max<std::uint64_t>(endAddress, static_cast<std::uint64_t>(m_memory.size()) * 2);
   m_memory.resize(static_cast<std::uint32_t>(std::min<std::uint64_t>(grown, m_memoryLimitTrytes)), 0);
}

void ExecutableProgram::loadFromHeap(std::int32_t address, std::uint8_t numTrytes, Trytes& out) const
{
   if((numTrytes > 4))
   {
      throw RebelError("unsupported load size " + std::to_string(numTrytes));
   }

   if(address < static_cast<std::int32_t>(getInstructionsSizeTrytes()))
   {
      throw std::out_of_range("heap address below the instruction image: " + std::to_string(address));
   }
   std::uint32_t heapAddress = address - getInstructionsSizeTrytes() /* +/- program start address if it's other than 0 */;
   if(static_cast<std::uint64_t>(heapAddress) + numTrytes > m_memory.size())
   {
      throw std::out_of_range("heap read past the program image: " + std::to_string(address));
   }
   for(std::uint8_t i = 0; i < numTrytes; ++i)
   {
      out.push_back(m_memory[heapAddress]);
      // std::cout << "Loading from memory = " << std::hex << m_heap[heapAddress] << std::dec << " at index " << heapAddress << std::endl;
       ++heapAddress;

   }
 }

std::int32_t ExecutableProgram::loadSymbolAddress(const std::string& symbolName) const
{
   try
   {
      return m_symbolTable.at(symbolName);
   }
   catch(std::exception&)
   {
      // Fall through to symbol+addend parsing.
   }

   const std::size_t pos = symbolName.find_last_of("+-");
   if(pos != std::string::npos && pos > 0)
   {
      const std::int32_t base = loadSymbolAddress(symbolName.substr(0, pos));
      const std::int32_t addend = std::stoi(symbolName.substr(pos));
      return base + addend;
   }

   throw std::out_of_range("symbol not found: " + symbolName);
}

void ExecutableProgram::storeToHeap(std::int32_t address, const Tint& value, std::uint8_t numTrytes)
{
   if((numTrytes > 4))
   {
      throw RebelError("unsupported store size " + std::to_string(numTrytes));
   }

   if(address < static_cast<std::int32_t>(getInstructionsSizeTrytes()))
   {
      throw std::out_of_range("heap address below the instruction image: " + std::to_string(address));
   }
   std::uint32_t heapAddress = address - getInstructionsSizeTrytes() /* +/- some value if program starts at address other than 0 */;

   doStoreToHeap(heapAddress, value, numTrytes);
}

void ExecutableProgram::calculateMemorySize()
{
   if(m_heapSizeTrytes > m_memoryLimitTrytes)
   {
      throw std::length_error(
         "Program needs " + std::to_string(m_heapSizeTrytes) + " heap trytes, memory limit is "
         + std::to_string(m_memoryLimitTrytes));
   }

   m_memory.resize(m_heapSizeTrytes, 0);
}

std::uint32_t ExecutableProgram::getProgramSizeTrytes() const
{
   return getInstructionsSizeTrytes() + m_heapSizeTrytes;
}

std::uint32_t ExecutableProgram::getHeapSizeTrytes() const
{
   return m_heapSizeTrytes;
}

std::uint32_t ExecutableProgram::getMemoryLimitTrytes() const
{
   return m_memoryLimitTrytes;
}

void ExecutableProgram::setMemoryLimitTrytes(std::uint32_t limit)
{
   m_memoryLimitTrytes = limit;
}

std::uint32_t ExecutableProgram::getInstructionsSizeTrytes() const
{
   std::uint32_t retVal = std::ceil((m_instructionsSize * (float)REBEL6_TRITS_PER_INSTRUCTION) / REBEL6_TRITS_PER_TRYTE);
   // std::cout << "Instruction size trytes first = " << retVal <<std::endl;
   retVal = retVal + 4 - (retVal % 4); // Make it 4 tryte aligned so it'll work better with RV32I pointer arithmetic
   // std::cout << "Instruction size trytes  = " << retVal << " is multiple of 4 = " << (retVal % 4) << "; m_instructionsSize = " << m_instructionsSize <<std::endl;

   return retVal;
}

void ExecutableProgram::printInstructions() const
{
   for(std::uint32_t pc = 0; pc < m_instructions.size(); ++pc)
   {
      std::cout << pc << ": " << *m_instructions[pc] << std::endl;
   }
}

void ExecutableProgram::printSymbols() const
{
   for(const auto& [key, value]: m_symbolTable)
   {
      std::cout << key << ": " << value << std::endl;
   }
}

void ExecutableProgram::doStoreToHeap(std::uint32_t heapAddress, const Tint& value, std::uint8_t numTrytes)
{
   static thread_local Trits trits;
   trits.clear();
   TernaryLogic::tintToTrits(value, trits); // Could send m_heap directly, but lets keep it safe
   std::uint8_t numTrits = numTrytes * REBEL6_TRITS_PER_TRYTE;

   if(trits.size() > numTrits)
   {
      std::cout << "Storing an unintended number of trits to heap - Intended = " << (int)numTrits << "; actual = " << trits.size() << std::endl;
   }

   trits.resize(numTrits, 0); // Fill the rest with zeroes so we ensure the memory allocated for this value has all trits properly reset

   ensureHeapCapacity(static_cast<std::uint64_t>(heapAddress) + numTrytes);

   for(std::uint8_t i = 0; i < numTrytes; ++i)
   {
      Tryte val = 0;
      for(std::uint8_t j = 0; j < REBEL6_TRITS_PER_TRYTE; ++j)
      {
         val += (trits[j + (i * REBEL6_TRITS_PER_TRYTE)] * POW3[j]);
      }

      m_memory[heapAddress] = val;
      // std::cout << "Storing ternary value to heap = " << std::hex << val << std::dec << " at index " << heapAddress << std::endl;
      ++heapAddress;
   }
   // std::cout << std::endl;
}

}
}
