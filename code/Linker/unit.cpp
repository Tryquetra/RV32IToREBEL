#include "unit.h"

#include <Expressions/all_expressions.h>
#include <Parsers/lexer.h>
#include <Parsers/parser.h>
#include <Translators/translator.h>

#include <cctype>
#include <iterator>
#include <fstream>
#include <sstream>

namespace
{
bool isSymbolStart(char c)
{
   return std::isalpha(static_cast<unsigned char>(c)) || (c == '_') || (c == '.') || (c == '$');
}

bool isSymbolChar(char c)
{
   return std::isalnum(static_cast<unsigned char>(c)) || (c == '_') || (c == '.') || (c == '$');
}

// Replaces whole identifier tokens, leaving registers and string literals alone.
std::string renameTokens(const std::string& text, const std::map<std::string, std::string>& names)
{
   std::string out;
   std::size_t i = 0;
   while(i < text.size())
   {
      const char c = text[i];
      if(c == '"')
      {
         std::size_t j = i + 1;
         while(j < text.size())
         {
            if(text[j] == '\\')
            {
               j += 2;
               continue;
            }
            if(text[j] == '"')
            {
               ++j;
               break;
            }
            ++j;
         }
         out.append(text, i, j - i);
         i = j;
         continue;
      }
      if(isSymbolStart(c))
      {
         std::size_t j = i + 1;
         while((j < text.size()) && isSymbolChar(text[j]))
         {
            ++j;
         }
         const std::string token = text.substr(i, j - i);
         const auto found = names.find(token);
         out += (found == names.end()) ? token : found->second;
         i = j;
         continue;
      }
      out += c;
      ++i;
   }
   return out;
}
}

namespace Linker
{
void applyDebugDirectives(Expressions::ExpressionList& expressions)
{
   std::map<int, std::string> files;
   std::string currentFile;
   int currentLine = 0;
   for(auto it = expressions.begin(); it != expressions.end();)
   {
      Expressions::Expression* expr = *it;
      if(expr->getExpressionType() == Expressions::Expression::ExpressionType::DIRECTIVE)
      {
         Expressions::Directive* directive = static_cast<Expressions::Directive*>(expr);
         const std::string& name = directive->getDirectiveName();
         const std::vector<std::string>& params = directive->getDirectiveParameters();
         if(name == ".file")
         {
            if(params.size() >= 2)
            {
               std::string value = params[1];
               if((value.size() >= 2) && (value.front() == '"') && (value.back() == '"'))
               {
                  value = value.substr(1, value.size() - 2);
               }
               files[std::stoi(params[0])] = value;
            }
            it = expressions.erase(it);
            delete expr;
            continue;
         }
         if(name == ".loc")
         {
            if(!params.empty())
            {
               const int index = std::stoi(params[0]);
               const auto found = files.find(index);
               currentFile = (found == files.end()) ? std::string() : found->second;
               currentLine = (params.size() >= 2) ? std::stoi(params[1]) : 0;
            }
            it = expressions.erase(it);
            delete expr;
            continue;
         }
      }
      expr->setCSource(currentFile, currentLine);
      ++it;
   }

   // A label comes before the `.loc` of its own body, so it takes the nearest following origin.
   for(auto it = expressions.begin(); it != expressions.end(); ++it)
   {
      if((*it)->getExpressionType() != Expressions::Expression::ExpressionType::LABEL)
      {
         continue;
      }
      auto ahead = std::next(it);
      for(int step = 0; (step < 3) && (ahead != expressions.end()); ++step, ++ahead)
      {
         if((*ahead)->getCLine() > 0)
         {
            (*it)->setCSource((*ahead)->getCFile(), (*ahead)->getCLine());
            break;
         }
      }
   }
}
}

namespace Linker
{
Unit::Unit(std::string name)
   : m_name(std::move(name))
{
}

Unit Unit::fromAssembly(const std::string& name, const std::string& source)
{
   Lexer lexer(source);
   Parser parser(&lexer);
   Expressions::ExpressionList parsed;
   parser.parse(parsed);
   applyDebugDirectives(parsed);

   Unit unit(name);
   Translators::Translator::Translate("rebel-6", parsed, unit.m_expressions);
   for(Expressions::Expression* expr: parsed)
   {
      delete expr;
   }
   unit.collectSymbols();
   unit.renameSymbols();
   return unit;
}

Unit Unit::fromObject(const std::string& name, const std::string& source)
{
   std::istringstream stream(source);
   std::string line;
   std::string body;
   bool inBody = false;
   while(std::getline(stream, line))
   {
      if(!inBody)
      {
         if(line == "body")
         {
            inBody = true;
         }
         continue;
      }
      if(line == "end")
      {
         break;
      }
      body += line;
      body += '\n';
   }
   if(!inBody)
   {
      throw RebelError("object " + name + " has no body");
   }

   Lexer lexer(body);
   Parser parser(&lexer);
   Unit unit(name);
   parser.parse(unit.m_expressions);
   unit.collectSymbols();
   return unit;
}

void Unit::collectSymbols()
{
   m_globals.clear();
   m_locals.clear();
   m_defined.clear();
   std::set<std::string>& defined = m_defined;
   for(const Expressions::Expression* expr: m_expressions)
   {
      switch(expr->getExpressionType())
      {
      case Expressions::Expression::ExpressionType::LABEL:
         defined.insert(static_cast<const Expressions::Label*>(expr)->getLabelName());
         break;
      case Expressions::Expression::ExpressionType::DIRECTIVE:
      {
         const Expressions::Directive* directive = static_cast<const Expressions::Directive*>(expr);
         const std::string& name = directive->getDirectiveName();
         if((name == ".globl") || (name == ".global") || (name == ".weak"))
         {
            for(const std::string& symbol: directive->getDirectiveParameters())
            {
               m_globals.insert(symbol);
            }
         }
         else if((name == ".comm") || (name == ".lcomm"))
         {
            const std::vector<std::string>& params = directive->getDirectiveParameters();
            if(!params.empty())
            {
               defined.insert(params[0]);
            }
         }
         break;
      }
      default:
         break;
      }
   }

   for(const std::string& symbol: defined)
   {
      if(m_globals.count(symbol) == 0)
      {
         m_locals.insert(symbol);
      }
   }
}

void Unit::renameSymbols()
{
   if(m_locals.empty())
   {
      return;
   }

   std::map<std::string, std::string> names;
   for(const std::string& symbol: m_locals)
   {
      names[symbol] = "." + m_name + "_" + symbol;
   }

   for(Expressions::Expression* expr: m_expressions)
   {
      switch(expr->getExpressionType())
      {
      case Expressions::Expression::ExpressionType::LABEL:
      {
         Expressions::Label* label = static_cast<Expressions::Label*>(expr);
         const auto found = names.find(label->getLabelName());
         if(found != names.end())
         {
            label->setLabelName(found->second);
         }
         break;
      }
      case Expressions::Expression::ExpressionType::INSTRUCTION:
      {
         Expressions::Instruction* instr = static_cast<Expressions::Instruction*>(expr);
         for(std::string& operand: instr->getInstructionOperands())
         {
            operand = renameTokens(operand, names);
         }
         break;
      }
      case Expressions::Expression::ExpressionType::DIRECTIVE:
      {
         Expressions::Directive* directive = static_cast<Expressions::Directive*>(expr);
         const std::string& name = directive->getDirectiveName();
         if((name == ".globl") || (name == ".global") || (name == ".weak") || (name == ".section"))
         {
            break;
         }
         for(std::string& parameter: directive->getDirectiveParameters())
         {
            if(name == ".string")
            {
               continue;
            }
            parameter = renameTokens(parameter, names);
         }
         break;
      }
      default:
         break;
      }
   }
}

void Unit::writeObject(const std::string& path, const std::map<std::string, std::int32_t>& symbols,
                       const std::vector<std::string>& relocations, std::uint32_t bssSize) const
{
   std::ofstream file(path);
   if(!file)
   {
      throw RebelError("cannot write object " + path);
   }
   file << "MTO 1\n";
   file << "unit " << m_name << "\n";
   file << "target rebel-6\n";
   file << "bss " << bssSize << "\n";
   for(const auto& [name, address]: symbols)
   {
      file << "symbol " << name << " " << address << " " << (m_globals.count(name) ? "global" : "local") << "\n";
   }
   for(const std::string& relocation: relocations)
   {
      file << "reloc " << relocation << "\n";
   }
   file << "body\n";
   for(const Expressions::Expression* expr: m_expressions)
   {
      file << *expr << "\n";
   }
   file << "end\n";
}
}
