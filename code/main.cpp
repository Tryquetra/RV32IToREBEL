#include "Linker/unit.h"

#include "Parsers/lexer.h"
#include "Parsers/token.h"
#include "Parsers/parser.h"

#include "Expressions/expression.h"
#include "Expressions/all_expressions.h"

#include "Translators/translator.h"

#include "Assemblers/REBEL6/assembler.h"
#include "Assemblers/REBEL6/executableprogram.h"

#include <errors.h>

#include <cstdint>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace
{
struct Options
{
   std::vector<std::string> inputs;
   std::string output;
   std::string target = "rebel-6";
   std::uint32_t memory = 0;
   bool emitObject = false;
};

struct AssemblyResult
{
   std::map<std::string, std::int32_t> symbols;
   std::uint32_t bss = 0;
};

Options parseOptions(int argc, char* argv[])
{
   Options options;
   for(int i = 1; i < argc; ++i)
   {
      const std::string arg = argv[i];
      if((arg == "-o") || (arg == "--output"))
      {
         if(i + 1 >= argc)
         {
            throw RebelError(arg + " needs a path");
         }
         options.output = argv[++i];
      }
      else if(arg == "--emit-obj")
      {
         options.emitObject = true;
      }
      else if(arg == "--target")
      {
         if(i + 1 >= argc)
         {
            throw RebelError("--target needs a name");
         }
         options.target = argv[++i];
      }
      else if(arg == "--memory")
      {
         if(i + 1 >= argc)
         {
            throw RebelError("--memory needs a size in trytes");
         }
         options.memory = static_cast<std::uint32_t>(std::stoul(argv[++i]));
      }
      else if((arg == "-h") || (arg == "--help"))
      {
         std::cout << "usage: RV32IToREBEL [options] <file.s|file.mto>...\n"
                      "  -o, --output <path>   output name (without extension)\n"
                      "  --emit-obj            write a .mto object per input\n"
                      "  --target <isa>        translation target (rebel-6)\n"
                      "  --memory <trytes>     heap limit, default 8 MiB\n";
         std::exit(0);
      }
      else if(arg == "--version")
      {
         std::cout << "RV32IToREBEL 0.2\n";
         std::exit(0);
      }
      else if(!arg.empty() && (arg[0] == '-'))
      {
         throw RebelError("unknown option " + arg);
      }
      else
      {
         options.inputs.push_back(arg);
      }
   }
   return options;
}

std::string readFile(const std::string& path)
{
   std::ifstream file(path);
   if(!file)
   {
      throw RebelError("cannot open " + path);
   }
   std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
   source += '\n';
   return source;
}

std::string stemOf(const std::string& path)
{
   return std::filesystem::path(path).stem().string();
}

bool isSymbolToken(const std::string& text)
{
   if(text.empty() || (!std::isalpha(static_cast<unsigned char>(text[0])) && (text[0] != '_') && (text[0] != '.') && (text[0] != '$')))
   {
      return false;
   }
   for(char c: text)
   {
      if(!std::isalnum(static_cast<unsigned char>(c)) && (c != '_') && (c != '.') && (c != '$'))
      {
         return false;
      }
   }
   return true;
}

std::vector<std::string> collectRelocations(const Linker::Unit& unit)
{
   std::vector<std::string> relocations;
   std::int64_t pc = 0;
   std::int64_t data = 0;
   for(const Expressions::Expression* expr: unit.getExpressions())
   {
      if(expr->getExpressionType() == Expressions::Expression::ExpressionType::INSTRUCTION)
      {
         const Expressions::Instruction* instr = static_cast<const Expressions::Instruction*>(expr);
         for(const std::string& operand: instr->getInstructionOperands())
         {
            std::string symbol = operand;
            const std::size_t addend = symbol.find_first_of("+-");
            if(addend != std::string::npos)
            {
               symbol = symbol.substr(0, addend);
            }
            if(unit.getDefined().count(symbol) || unit.getGlobals().count(symbol))
            {
               relocations.push_back("text " + std::to_string(pc) + " " + symbol);
            }
         }
         ++pc;
      }
      else if(expr->getExpressionType() == Expressions::Expression::ExpressionType::DIRECTIVE)
      {
         const Expressions::Directive* directive = static_cast<const Expressions::Directive*>(expr);
         const std::string& name = directive->getDirectiveName();
         const std::vector<std::string>& params = directive->getDirectiveParameters();
         if(name == ".word")
         {
            for(const std::string& param: params)
            {
               if(unit.getDefined().count(param) || unit.getGlobals().count(param))
               {
                  relocations.push_back("data " + std::to_string(data) + " " + param);
               }
               data += 4;
            }
         }
         else if(name == ".half")
         {
            data += 2 * static_cast<std::int64_t>(params.size());
         }
         else if(name == ".byte")
         {
            data += static_cast<std::int64_t>(params.size());
         }
         else if(name == ".zero")
         {
            if(!params.empty())
            {
               data += std::stoll(params[0]);
            }
         }
      }
   }
   return relocations;
}

AssemblyResult assemble(Expressions::ExpressionList& expressions, const std::string& base, bool writeTas,
                        std::uint32_t memory)
{
   Assemblers::REBEL6::ExecutableProgram program;
   if(memory != 0)
   {
      program.setMemoryLimitTrytes(memory);
   }
   Assemblers::REBEL6::Assembler assembler(expressions, program);
   if(writeTas)
   {
      assembler.printExpressionsToFile(base);
   }
   assembler.init();
   assembler.run();

   AssemblyResult result;
   result.symbols = program.getSymbols();
   result.bss = assembler.getBssSizeTrytes();
   return result;
}

int singleUnit(const Options& options)
{
   const std::string& input = options.inputs.front();
   const std::string source = readFile(input);
   const std::string stem = stemOf(input);

   if(input.size() > 4 && input.compare(input.size() - 4, 4, ".mto") == 0)
   {
      Linker::Unit unit = Linker::Unit::fromObject(stem, source);
      Expressions::ExpressionList& expressions = unit.getExpressions();
      assemble(expressions, options.output.empty() ? stem : options.output, true, options.memory);
      return 0;
   }

   if(options.emitObject)
   {
      Linker::Unit unit = Linker::Unit::fromAssembly(stem, source);
      const std::vector<std::string> relocations = collectRelocations(unit);
      AssemblyResult result = assemble(unit.getExpressions(), stem, false, options.memory);
      unit.writeObject(stem + ".mto", result.symbols, relocations, result.bss);
      return 0;
   }

   // Plain single-file path: translate without unit scoping, exactly as before.
   Lexer lexer(source);
   Parser parser(&lexer);
   Expressions::ExpressionList parsed;
   parser.parse(parsed);

   Expressions::ExpressionList translated;
   Translators::Translator::Translate(options.target, parsed, translated);
   for(Expressions::Expression* expr: parsed)
   {
      delete expr;
   }

   assemble(translated, options.output.empty() ? stem : options.output, true, options.memory);
   return 0;
}

int run(int argc, char* argv[])
{
   const Options options = parseOptions(argc, argv);
   if(options.inputs.empty())
   {
      std::cerr << "usage: RV32IToREBEL [options] <file.s|file.mto>...\n";
      return 2;
   }
   if(options.inputs.size() > 1)
   {
      throw RebelError("multiple inputs need the linker");
   }
   return singleUnit(options);
}
}

int main(int argc, char* argv[])
{
   try
   {
      return run(argc, argv);
   }
   catch(const std::exception& error)
   {
      std::cerr << "error: " << error.what() << std::endl;
      return 2;
   }
}
