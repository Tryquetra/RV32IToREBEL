#include "translator.h"

#include "rv32itorv32ibase.h"
#include "rv32itorebel6.h"

#include <Expressions/all_expressions.h>
#include <Parsers/expressionparser.h>
#include <Parsers/parseutils.h>
#include <logger.h>

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <errors.h>

namespace
{
const Translators::RV32IToRV32IBase rv32i;
const Translators::RV32IToREBEL6 rebel6;

using Arity = std::pair<int, int>; // Minimum and maximum operand count.

const std::unordered_map<std::string, Arity>& arities()
{
   static const std::unordered_map<std::string, Arity> table = {
      {"fence", {0, 2}}, {"ecall", {0, 0}}, {"ebreak", {0, 0}}, {"nop", {0, 0}}, {"ret", {0, 0}},
      {"j", {1, 1}}, {"jr", {1, 1}}, {"call", {1, 1}}, {"jal", {1, 2}}, {"jalr", {1, 3}},
      {"lui", {2, 2}}, {"auipc", {2, 2}}, {"la", {2, 2}}, {"lla", {2, 2}}, {"li", {2, 2}}, {"mv", {2, 2}},
      {"not", {2, 2}}, {"neg", {2, 2}}, {"seqz", {2, 2}}, {"snez", {2, 2}}, {"sltz", {2, 2}}, {"sgtz", {2, 2}},
      {"beqz", {2, 2}}, {"bnez", {2, 2}}, {"blez", {2, 2}}, {"bgez", {2, 2}}, {"bltz", {2, 2}}, {"bgtz", {2, 2}},
      {"lb", {2, 2}}, {"lh", {2, 2}}, {"lw", {2, 2}}, {"lbu", {2, 2}}, {"lhu", {2, 2}},
      {"sb", {2, 3}}, {"sh", {2, 3}}, {"sw", {2, 3}},
      {"add", {3, 3}}, {"sub", {3, 3}}, {"sll", {3, 3}}, {"slt", {3, 3}}, {"sltu", {3, 3}},
      {"xor", {3, 3}}, {"srl", {3, 3}}, {"sra", {3, 3}}, {"or", {3, 3}}, {"and", {3, 3}},
      {"addi", {3, 3}}, {"slti", {3, 3}}, {"sltiu", {3, 3}}, {"xori", {3, 3}}, {"ori", {3, 3}},
      {"andi", {3, 3}}, {"slli", {3, 3}}, {"srli", {3, 3}}, {"srai", {3, 3}},
      {"beq", {3, 3}}, {"bne", {3, 3}}, {"blt", {3, 3}}, {"bge", {3, 3}}, {"bltu", {3, 3}}, {"bgeu", {3, 3}},
      {"bgt", {3, 3}}, {"ble", {3, 3}}, {"bgtu", {3, 3}}, {"bleu", {3, 3}},
   };
   return table;
}

void checkArity(const Expressions::Instruction& instr)
{
   const std::string& name = instr.getInstructionName();
   const auto entry = arities().find(name);
   if(entry == arities().end())
   {
      throw RebelError("unknown instruction \"" + name + "\"", instr.getLocation());
   }

   const std::size_t count = instr.getInstructionOperands().size();
   const std::size_t min = static_cast<std::size_t>(entry->second.first);
   const std::size_t max = static_cast<std::size_t>(entry->second.second);
   if((count < min) || (count > max))
   {
      const std::string expected = (min == max) ? std::to_string(min) : (std::to_string(min) + " to " + std::to_string(max));
      throw RebelError("instruction \"" + name + "\" takes " + expected + " operands, got " + std::to_string(count), instr.getLocation());
   }
}

bool isBranchOrJump(const std::string& name)
{
   static const std::unordered_set<std::string> names = {
      "beq", "bne", "blt", "bge", "bltu", "bgeu",
      "beq.t", "bne.t", "blt.t", "bge.t", "jal", "jal.t",
   };
   return names.count(name) > 0;
}

// RV32I offsets are byte offsets; REBEL-6 branch and jump offsets count instruction slots.
void normalizeBranchOffset(Expressions::Expression* expr)
{
   if(expr->getExpressionType() != Expressions::Expression::ExpressionType::INSTRUCTION)
   {
      return;
   }
   Expressions::Instruction* instr = static_cast<Expressions::Instruction*>(expr);
   if(!isBranchOrJump(instr->getInstructionName()))
   {
      return;
   }
   std::vector<std::string>& operands = instr->getInstructionOperands();
   if(operands.empty())
   {
      return;
   }
   std::int64_t offset = 0;
   if(!ParseUtils::parseInteger(operands.back(), offset))
   {
      return; // A symbol, resolved by the assembler.
   }
   if((offset % 4) != 0)
   {
      throw RebelError("offset " + operands.back() + " is not a multiple of 4", instr->getLocation());
   }
   operands.back() = std::to_string(offset / 4);
}

void resolveInstruction(const Expressions::Instruction* instr, Expressions::ExpressionList& out, const Translators::InstructionTranslatorBase& translator)
{
   checkArity(*instr);
   const std::string& name = instr->getInstructionName();
   const std::vector<std::string>& operands = instr->getInstructionOperands();

   try
   {
      translator.at(name)(operands, out);
   }
   catch(const std::exception&e)
   {
      throw RebelError("cannot translate \"" + instr->toString() + "\": " + e.what(), instr->getLocation());
   }
}
}

namespace Translators
{
void Translator::Translate(const std::string& targetISAName, const Expressions::ExpressionList& from, Expressions::ExpressionList& to)
{
   const InstructionTranslatorBase* translator = nullptr;
   ExpressionParser parser(from);
   std::string isaName;
   isaName.resize(targetISAName.length());
   std::transform(targetISAName.begin(), targetISAName.end(), isaName.begin(), [](std::uint8_t c){ return std::tolower(c); });

   if(isaName == "rv32i")
   {
      translator = &rv32i;
   }
   else if((isaName == "rebel6") || (isaName == "rebel-6"))
   {
      translator = &rebel6;
   }
   else
   {
      throw RebelError("unsupported target ISA: " + targetISAName);
   }

   const Expressions::Expression* expr = parser.nextExpression();
   while(expr != nullptr)
   {
      switch(expr->getExpressionType())
      {
      case Expressions::Expression::ExpressionType::DIRECTIVE:
      {
         const Expressions::Directive* dir = static_cast<const Expressions::Directive*>(expr);
         to.push_back(new Expressions::Directive(*dir));
         break;
      }
      case Expressions::Expression::ExpressionType::LABEL:
      {
         const Expressions::Label* label = static_cast<const Expressions::Label*>(expr);
         to.push_back(new Expressions::Label(*label));
         break;
      }
      case Expressions::Expression::ExpressionType::INSTRUCTION:
      {
         const Expressions::Instruction* instr = static_cast<const Expressions::Instruction*>(expr);
         const auto lastOld = to.empty() ? to.end() : std::prev(to.end());
         resolveInstruction(instr, to, *translator);
         // Generated instructions inherit the source position of the instruction they came from.
         for(auto it = (lastOld == to.end()) ? to.begin() : std::next(lastOld); it != to.end(); ++it)
         {
            if((*it)->getLocation().line == 0)
            {
               (*it)->setLocation(instr->getLocation());
            }
            normalizeBranchOffset(*it);
         }
         break;
      }
      case Expressions::Expression::ExpressionType::COMMENT:
         // Don't care
         break;

      default:
         // Don't care
         break;
      }

      expr = parser.nextExpression();
   }
}
}
