#include "encoder.h"
#include "rebel6spec.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

namespace
{
using Encoder::Spec;

std::int64_t pow3(std::uint8_t width)
{
   std::int64_t value = 1;
   for(std::uint8_t i = 0; i < width; ++i)
   {
      value *= 3;
   }
   return value;
}

void setTrit(std::uint64_t& word, std::size_t lane, std::int8_t trit)
{
   const std::uint64_t code = (trit == 1) ? 0b01 : ((trit == -1) ? 0b10 : 0b00);
   const std::size_t shift = lane * 2;
   word &= ~(std::uint64_t(0b11) << shift);
   word |= code << shift;
}

void putField(std::uint64_t& word, std::size_t offset, std::uint8_t width, std::int64_t value)
{
   const std::int64_t modulus = pow3(width);
   const std::int64_t half = (modulus - 1) / 2;
   std::int64_t v = value % modulus;
   if(v > half)
   {
      v -= modulus;
   }
   else if(v < -half)
   {
      v += modulus;
   }
   for(std::uint8_t i = 0; i < width; ++i)
   {
      const std::int64_t r = ((v % 3) + 3) % 3; // Euclidean remainder: 0, 1 or 2.
      const std::int8_t trit = (r == 0) ? 0 : ((r == 1) ? 1 : -1);
      v = (v - trit) / 3;
      setTrit(word, offset + i, trit);
   }
}

bool parseRegister(const std::string& name, std::int64_t& out)
{
   if((name.size() > 1) && (name[0] == 'x'))
   {
      char* end = nullptr;
      const long value = std::strtol(name.c_str() + 1, &end, 10);
      if((end != name.c_str() + 1) && (*end == '\0') && (value >= 0) && (value < 32))
      {
         out = value;
         return true;
      }
   }
   static const std::vector<std::pair<std::string, int>> names = {
      {"zero", 0}, {"ra", 1}, {"sp", 2}, {"gp", 3}, {"tp", 4},
      {"t0", 5}, {"t1", 6}, {"t2", 7}, {"s0", 8}, {"fp", 8}, {"s1", 9},
      {"a0", 10}, {"a1", 11}, {"a2", 12}, {"a3", 13}, {"a4", 14}, {"a5", 15}, {"a6", 16}, {"a7", 17},
      {"s2", 18}, {"s3", 19}, {"s4", 20}, {"s5", 21}, {"s6", 22}, {"s7", 23},
      {"s8", 24}, {"s9", 25}, {"s10", 26}, {"s11", 27},
      {"t3", 28}, {"t4", 29}, {"t5", 30}, {"t6", 31},
   };
   for(const auto& [candidate, index]: names)
   {
      if(name == candidate)
      {
         out = index;
         return true;
      }
   }
   return false;
}

bool parseNumber(const std::string& text, std::int64_t& out)
{
   if(text.empty())
   {
      return false;
   }
   char* end = nullptr;
   errno = 0;
   const long long value = std::strtoll(text.c_str(), &end, 0);
   if((end == text.c_str()) || (*end != '\0') || (errno == ERANGE))
   {
      return false;
   }
   // Match the assembler: values in (INT32_MAX, UINT32_MAX] are 32-bit unsigned constants.
   if((value > INT32_MAX) && (value <= UINT32_MAX))
   {
      out = static_cast<std::int32_t>(static_cast<std::uint32_t>(value));
   }
   else
   {
      out = value;
   }
   return true;
}

bool memoryOperand(const std::string& text, std::int64_t& base, std::int64_t& offset)
{
   const std::size_t open = text.find('(');
   if(open == std::string::npos)
   {
      return false;
   }
   if((text.empty()) || (text.back() != ')'))
   {
      return false;
   }
   const std::string baseName = text.substr(open + 1, text.size() - open - 2);
   if(!parseRegister(baseName, base))
   {
      return false;
   }
   const std::string offsetText = text.substr(0, open);
   if(offsetText.empty())
   {
      offset = 0;
      return true;
   }
   return parseNumber(offsetText, offset);
}

std::int64_t registerAt(const std::vector<std::string>& operands, std::size_t index, bool& ok)
{
   std::int64_t value = 0;
   if((index < operands.size()) && parseRegister(operands[index], value))
   {
      return value;
   }
   ok = false;
   return 0;
}

std::int64_t numberAt(const std::vector<std::string>& operands, std::size_t index, bool& ok)
{
   std::int64_t value = 0;
   if((index < operands.size()) && parseNumber(operands[index], value))
   {
      return value;
   }
   ok = false;
   return 0;
}

std::int64_t balancedLow(std::int64_t value, std::uint8_t width)
{
   const std::int64_t modulus = pow3(width);
   const std::int64_t half = (modulus - 1) / 2;
   std::int64_t low = ((value % modulus) + modulus) % modulus;
   if(low > half)
   {
      low -= modulus;
   }
   return low;
}
}

namespace Encoder
{
bool encode(const Expressions::Instruction& instr, std::uint64_t& word, std::string& error)
{
   std::string name = instr.getInstructionName();
   if(name == "ecall") name = "ecall.t";
   else if(name == "ebreak") name = "ebreak.t";
   else if(name == "fence") name = "fence.t";
   else if(name == "slt") name = "slt.t";
   else if(name == "slti") name = "slti.t";
   const auto found = specs().find(name);
   if(found == specs().end())
   {
      error = "no encoding for \"" + name + "\"";
      return false;
   }
   const Spec& spec = found->second;
   const std::vector<std::string>& operands = instr.getInstructionOperands();

   std::int64_t rd1 = 0;
   std::int64_t rd2 = 0;
   std::int64_t rs1 = 0;
   std::int64_t rs2 = 0;
   std::int64_t rs3 = 0;
   std::int64_t imm = 0;
   std::int64_t imm2 = 0;
   bool ok = true;

   switch(spec.format)
   {
   case 'R':
      if(operands.empty())
      {
         ok = true; // ecall.t, ebreak.t, fence.t, wfi.t, tret.t
      }
      else
      {
         rd1 = registerAt(operands, 0, ok);
         rs1 = registerAt(operands, 1, ok);
         rs2 = registerAt(operands, 2, ok);
      }
      break;
   case 'I':
      rd1 = registerAt(operands, 0, ok);
      if((name == "lw") || (name == "lh") || (name == "lb") || (name == "lhu") || (name == "lbu")
         || (name == "lw.t") || (name == "lh.t") || (name == "lt.t") || (name == "jalr.t"))
      {
         ok = (operands.size() > 1) && memoryOperand(operands[1], rs1, imm);
      }
      else
      {
         rs1 = registerAt(operands, 1, ok);
         imm = numberAt(operands, 2, ok);
      }
      break;
   case 'B':
      if((name == "sw") || (name == "sh") || (name == "sb") || (name == "sw.t") || (name == "sh.t") || (name == "st.t"))
      {
         rs2 = registerAt(operands, 0, ok);
         ok = ok && (operands.size() > 1) && memoryOperand(operands[1], rs1, imm);
      }
      else if((name == "bcgs.t") || (name == "bceg.t"))
      {
         rs1 = registerAt(operands, 0, ok);
         rs2 = registerAt(operands, 1, ok);
         imm = numberAt(operands, 2, ok);
         imm2 = numberAt(operands, 3, ok);
      }
      else
      {
         rs1 = registerAt(operands, 0, ok);
         rs2 = registerAt(operands, 1, ok);
         imm = numberAt(operands, 2, ok);
      }
      break;
   case 'G':
      rd1 = registerAt(operands, 0, ok);
      imm = numberAt(operands, 1, ok);
      break;
   case 'X':
      rd1 = registerAt(operands, 0, ok);
      rd2 = registerAt(operands, 1, ok);
      imm = numberAt(operands, 2, ok);
      imm2 = numberAt(operands, 3, ok);
      break;
   case 'D':
      rd1 = registerAt(operands, 0, ok);
      rs1 = registerAt(operands, 1, ok);
      rs2 = registerAt(operands, 2, ok);
      rs3 = registerAt(operands, 3, ok);
      break;
   case 'Y':
      rs1 = registerAt(operands, 0, ok);
      imm = numberAt(operands, 1, ok);
      break;
   default:
      error = "unknown format for \"" + name + "\"";
      return false;
   }

   if(!ok)
   {
      error = "cannot parse operands of \"" + name + "\"";
      return false;
   }

   word = 0;
   switch(spec.format)
   {
   case 'R':
      putField(word, 0, 4, spec.opcode);
      putField(word, 4, 4, spec.func);
      putField(word, 8, 6, rd2);
      putField(word, 14, 6, rd1);
      putField(word, 20, 6, rs2);
      putField(word, 26, 6, rs1);
      break;
   case 'I':
      putField(word, 0, 4, spec.opcode);
      putField(word, 4, 4, spec.func);
      putField(word, 8, 6, balancedLow(imm, 6));
      putField(word, 14, 6, rd1);
      putField(word, 20, 6, (imm - balancedLow(imm, 6)) / pow3(6));
      putField(word, 26, 6, rs1);
      break;
   case 'B':
      putField(word, 0, 4, spec.opcode);
      putField(word, 4, 4, spec.func);
      if((name == "bcgs.t") || (name == "bceg.t"))
      {
         putField(word, 8, 6, imm2);
         putField(word, 14, 6, imm);
      }
      else
      {
         putField(word, 8, 12, imm);
      }
      putField(word, 20, 6, rs2);
      putField(word, 26, 6, rs1);
      break;
   case 'G':
      putField(word, 0, 2, spec.opcode);
      putField(word, 2, 12, balancedLow(imm, 12));
      putField(word, 14, 6, rd1);
      putField(word, 20, 12, (imm - balancedLow(imm, 12)) / pow3(12));
      break;
   case 'X':
      putField(word, 0, 4, spec.opcode);
      putField(word, 4, 4, spec.func);
      putField(word, 8, 6, rd2);
      putField(word, 14, 6, rd1);
      putField(word, 20, 6, imm2);
      putField(word, 26, 6, imm);
      break;
   case 'D':
      putField(word, 0, 4, spec.opcode);
      putField(word, 4, 4, spec.func);
      putField(word, 8, 6, rs3);
      putField(word, 14, 6, rd1);
      putField(word, 20, 6, rs2);
      putField(word, 26, 6, rs1);
      break;
   case 'Y':
      putField(word, 0, 2, spec.opcode);
      putField(word, 2, 24, imm);
      putField(word, 26, 6, rs1);
      break;
   default:
      break;
   }
   return true;
}

bool writeImage(const std::string& path, const Assemblers::REBEL6::ExecutableProgram& program,
                std::int64_t entry, std::string& error)
{
   std::vector<std::uint64_t> words;
   const std::uint32_t count = program.getInstructionCount();
   std::uint8_t size = 0;
   for(std::uint32_t pc = 0; pc < count; pc += size)
   {
      const Expressions::Instruction* instr = program.loadInstruction(static_cast<std::int32_t>(pc), size);
      if(instr == nullptr)
      {
         error = "missing instruction at slot " + std::to_string(pc);
         return false;
      }
      std::uint64_t word = 0;
      if(!encode(*instr, word, error))
      {
         return false;
      }
      words.push_back(word);
   }

   const Trytes& data = program.getMemory();
   const std::int64_t dataBase = program.getInstructionsSizeTrytes();

   std::ofstream file(path, std::ios::binary);
   if(!file)
   {
      error = "cannot write " + path;
      return false;
   }
   const auto writeU64 = [&file](std::uint64_t value)
   {
      for(std::uint8_t i = 0; i < 8; ++i)
      {
         file.put(static_cast<char>((value >> (8 * i)) & 0xff));
      }
   };
   file.write("R6IM", 4);
   writeU64(words.size());
   writeU64(data.size());
   writeU64(static_cast<std::uint64_t>(dataBase));
   writeU64(static_cast<std::uint64_t>(entry));
   for(std::uint64_t word: words)
   {
      writeU64(word);
   }
   for(std::int16_t cell: data)
   {
      file.put(static_cast<char>(cell & 0xff));
      file.put(static_cast<char>((cell >> 8) & 0xff));
   }
   return static_cast<bool>(file);
}
}
