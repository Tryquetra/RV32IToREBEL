#pragma once

#include "expression.h"

namespace Expressions
{
class Label : public Expression
{
public:

   Label(const std::string& labelName, SourceLocation location = {});
   virtual ~Label() = default;

   virtual ExpressionType getExpressionType() const;
   virtual std::ostream& printToStream(std::ostream& stream) const;

   const std::string& getLabelName() const;
   void setLabelName(const std::string& labelName) { m_name = labelName; }

private:
   std::string m_name;
};
}
