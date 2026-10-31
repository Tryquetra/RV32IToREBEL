#pragma once

#include <Expressions/expression.h>
#include <errors.h>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace Linker
{
// One relocatable unit: translated REBEL-6 assembly with unit-local symbols renamed.
class Unit
{
public:
   explicit Unit(std::string name);

   const std::string& getName() const { return m_name; }
   Expressions::ExpressionList& getExpressions() { return m_expressions; }
   const Expressions::ExpressionList& getExpressions() const { return m_expressions; }

   // Parses RV32I assembly, translates it to REBEL-6 and renames unit-local symbols.
   static Unit fromAssembly(const std::string& name, const std::string& source);
   // Reads a .mto object written by writeObject.
   static Unit fromObject(const std::string& name, const std::string& source);

   void writeObject(const std::string& path, const std::map<std::string, std::int32_t>& symbols,
                    const std::vector<std::string>& relocations, std::uint32_t bssSize) const;

   const std::set<std::string>& getGlobals() const { return m_globals; }
   const std::set<std::string>& getLocals() const { return m_locals; }
   const std::set<std::string>& getDefined() const { return m_defined; }

private:
   void collectSymbols();
   void scopeSymbols();
   void renameSymbols();

   std::string m_name;
   Expressions::ExpressionList m_expressions;
   std::set<std::string> m_globals;
   std::set<std::string> m_locals;
   std::set<std::string> m_defined;
};
}
