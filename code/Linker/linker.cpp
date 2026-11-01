#include "linker.h"

#include <Expressions/all_expressions.h>

#include <fstream>
#include <map>

namespace Linker
{
Expressions::ExpressionList link(std::vector<Unit>& units)
{
   std::map<std::string, std::string> owners;
   for(Unit& unit: units)
   {
      for(const std::string& symbol: unit.getDefined())
      {
         if(unit.getGlobals().count(symbol) == 0)
         {
            continue;
         }
         const auto [entry, inserted] = owners.insert({symbol, unit.getName()});
         if(!inserted)
         {
            throw RebelError("duplicate global \"" + symbol + "\" in " + entry->second + " and " + unit.getName());
         }
      }
   }

   Expressions::ExpressionList merged;
   for(Unit& unit: units)
   {
      merged.splice(merged.end(), unit.getExpressions());
   }
   return merged;
}

void writeMap(const std::string& path, const std::vector<Unit>& units)
{
   std::ofstream file(path);
   if(!file)
   {
      throw RebelError("cannot write " + path);
   }
   file << "# unit\tlinker name\tsource\n";
   for(std::size_t index = 0; index < units.size(); ++index)
   {
      const Unit& unit = units[index];
      file << "# unit " << index << "\t" << unit.getName() << "\t"
           << (unit.getPath().empty() ? std::string("<generated>") : unit.getPath()) << "\n";
      file << "# asm_line\tc_line\tc_file\n";
      for(const Expressions::Expression* expr: unit.getExpressions())
      {
         const int line = expr->getLocation().line;
         if(line == 0)
         {
            continue;
         }
         file << line << "\t" << expr->getCLine() << "\t" << expr->getCFile() << "\n";
      }
   }
}

void writeSymbols(const std::string& path, const std::vector<Unit>& units)
{
   std::ofstream file(path);
   if(!file)
   {
      throw RebelError("cannot write " + path);
   }
   file << "# emitted\tscope\tunit\tc_line\tc_file\toriginal\n";
   for(std::size_t index = 0; index < units.size(); ++index)
   {
      const Unit& unit = units[index];
      std::map<std::string, const Expressions::Expression*> labels;
      for(const Expressions::Expression* expr: unit.getExpressions())
      {
         if(expr->getExpressionType() == Expressions::Expression::ExpressionType::LABEL)
         {
            const auto* label = static_cast<const Expressions::Label*>(expr);
            labels[label->getLabelName()] = expr;
         }
      }
      for(const std::string& name: unit.getDefined())
      {
         if((name.size() >= 2) && (name.compare(0, 2, ".L") == 0))
         {
            // Compiler-generated labels carry no name a reader would search for.
            continue;
         }
         const bool global = unit.getGlobals().count(name) > 0;
         const std::string emitted = global ? name : ("." + unit.getName() + "_" + name);
         const auto found = labels.find(emitted);
         const int line = (found == labels.end()) ? 0 : found->second->getCLine();
         const std::string where = (found == labels.end()) ? std::string() : found->second->getCFile();
         file << emitted << "\t" << (global ? "global" : "local") << "\t" << index << "\t" << line << "\t"
              << where << "\t" << name << "\n";
      }
      for(const std::string& name: unit.getGlobals())
      {
         if(unit.getDefined().count(name) == 0)
         {
            file << name << "\tglobal\t" << index << "\t0\t\t" << name << "\n";
         }
      }
   }
}
}
