#pragma once


#include <Parsers/expressionparser.h>
#include <Parsers/parseutils.h>
#include <Parsers/ternarylogic.h>
#include <Assemblers/directivehelper.h>
#include <Assemblers/REBEL6/assemblerutils.h>

#include <vector>
#include <map>

namespace Assemblers
{
namespace REBEL6
{

class ExecutableProgram;

class Assembler
{
public:
  Assembler(Expressions::ExpressionList& expressions, ExecutableProgram& executable);

  void init();
  void run();

  void printExpressionsToFile(const std::string& fileName);
  std::uint32_t getBssSizeTrytes() const { return static_cast<std::uint32_t>(m_bssSizeTrytes); }
private:
  void handleTextSection(const Expressions::Expression* expr);
  void handleDataSection(const Expressions::Expression* expr);
  void handleBssSection(const Expressions::Expression* expr);
  void handleRoDataSection(const Expressions::Expression* expr);

  void resolveDataDirective(const Expressions::Directive* directive);
  bool handleAlignment(const Expressions::Directive* directive, std::int32_t& cursor);

  bool resolveIfObject(const Expressions::Directive* directive, std::uint8_t& tryteSizePerElement, std::vector<Tint>& values);

  void resolveOperands();
  void checkImmediateRange(const Expressions::Instruction& instr, AssemblerUtils::InstructionType type);
  void resolveUnresolvedHeapData();
  std::int32_t resolveAssemblerModifier(const ParseUtils::ASSEMBLER_MODIFIER& modifier, const std::string& imm, std::int32_t pc);

  ExpressionParser m_parser;
  DirectiveHelper::SectionType m_sectionType;
  ExecutableProgram& m_executable;

  using HeapLabelMap = std::map<std::string, std::int32_t>; // Label | address
  HeapLabelMap m_tempHeapLabels;
  std::map<std::string, SourceLocation> m_labelLocations;
  std::int32_t m_pc; // Program counter
  std::int32_t m_hc; // Heap counter - used to generate the correct value for labels related to data/bss/rodata objects
  std::int32_t m_bssSizeTrytes; // .bss is laid out after .data and is not part of the initialized image
  std::map<std::string, std::int32_t> m_bssLabels; // Label | offset inside .bss

  struct RelocationItem
  {
     std::int32_t address;
     std::uint8_t sizeInTrytes;
     std::string label;
     bool isBinary; // Binary values are stored differently in memory
  };
  using HeapRelocationVector = std::vector<RelocationItem>;
  HeapRelocationVector m_heapRelocations;
};

}
}
