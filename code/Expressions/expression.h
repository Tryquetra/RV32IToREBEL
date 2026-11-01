#pragma once

#include <string>
#include <list>
#include <sstream>

#include <errors.h>

namespace Expressions
{

class Expression
{
public:
   enum class ExpressionType{DIRECTIVE, INSTRUCTION, LABEL, COMMENT, UNDEFINED};

   Expression() = default;
   virtual ~Expression() = default;

   virtual ExpressionType getExpressionType() const = 0;
   virtual std::ostream& printToStream(std::ostream& stream) const = 0;

   std::string toString() const
   {
      std::ostringstream stream;
      printToStream(stream);
      return stream.str();
   }

   SourceLocation getLocation() const { return m_location; }
   void setLocation(const SourceLocation& location) { m_location = location; }

   // Origin in the C source, filled from `.loc` and `.file` by the linker.
   const std::string& getCFile() const { return m_cFile; }
   int getCLine() const { return m_cLine; }
   void setCSource(const std::string& file, int line)
   {
      m_cFile = file;
      m_cLine = line;
   }

protected:
   SourceLocation m_location;
   std::string m_cFile;
   int m_cLine = 0;
};


using ExpressionList = std::list<Expression*>;
}

inline std::ostream& operator<<(std::ostream& file, const Expressions::Expression& expr)
{
   return expr.printToStream(file);
}
