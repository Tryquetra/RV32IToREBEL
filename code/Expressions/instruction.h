#pragma once

#include "expression.h"

#include <cstdint>
#include <vector>

namespace Expressions
{
class Instruction : public Expression
{
public:
   Instruction(const std::string& instructionName, const std::vector<std::string>& instructionOperands, SourceLocation location = {});
   virtual ~Instruction() = default;

   virtual ExpressionType getExpressionType() const;
   virtual std::ostream& printToStream(std::ostream& stream) const;

   const std::string& getInstructionName() const;
   std::uint8_t getType() const { return m_type; }
   void setType(std::uint8_t type) { m_type = type; }
   const std::vector<std::string>& getInstructionOperands()const ;
   std::vector<std::string>& getInstructionOperands();

private:

   std::string m_name;
   std::vector<std::string> m_operands;
   std::uint8_t m_type = 0;
};
}
