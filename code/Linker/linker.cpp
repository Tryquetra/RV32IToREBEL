#include "linker.h"

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
}
