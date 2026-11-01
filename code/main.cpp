#include "Encoder/encoder.h"
#include "Linker/linker.h"
#include "Linker/unit.h"

#include "Parsers/lexer.h"
#include "Parsers/token.h"
#include "Parsers/parser.h"

#include "Expressions/expression.h"
#include "Expressions/all_expressions.h"

#include "Translators/translator.h"

#include "Assemblers/REBEL6/assembler.h"
#include "Assemblers/REBEL6/executableprogram.h"
#include "Assemblers/RV32I/assembler.h"
#include "Assemblers/RV32I/executableprogram.h"
#include "Simulators/RV32I/simulator.h"
#include "Simulators/REBEL6/simulator.h"
#include "Simulators/REBEL6/testresult.h"

#include <errors.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
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
   std::string testDir;
   std::string statsPath;
   std::string mapPath;
   std::string symsPath;
   bool simulate = false;
   bool diff = false;
   bool emitObject = false;
   bool emitImage = false;
   bool link = false;
   bool check = false;
   bool noTas = false;
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
      else if(arg == "--emit-image")
      {
         options.emitImage = true;
      }
      else if(arg == "--check")
      {
         options.check = true;
      }
      else if(arg == "--no-tas")
      {
         options.noTas = true;
      }
      else if(arg == "--diff")
      {
         options.diff = true;
      }
      else if(arg == "--stats")
      {
         if(i + 1 >= argc)
         {
            throw RebelError("--stats needs a path");
         }
         options.statsPath = argv[++i];
      }
      else if(arg == "--simulate")
      {
         options.simulate = true;
      }
      else if(arg == "--run-tests")
      {
         if(i + 1 >= argc)
         {
            throw RebelError("--run-tests needs a directory");
         }
         options.testDir = argv[++i];
      }
      else if(arg == "--link")
      {
         options.link = true;
      }
      else if(arg == "--emit-map")
      {
         if(i + 1 >= argc)
         {
            throw RebelError("--emit-map needs a path");
         }
         options.mapPath = argv[++i];
      }
      else if(arg == "--emit-syms")
      {
         if(i + 1 >= argc)
         {
            throw RebelError("--emit-syms needs a path");
         }
         options.symsPath = argv[++i];
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
                      "  --link                link several units into one image\n"
                      "  --emit-map <path>     write the unit and line map\n"
                      "  --emit-syms <path>    write the symbol table with C lines\n"
                      "  --emit-obj            write a .mto object per input\n"
                      "  --emit-image          write an R6IM image next to the .tas\n"
                      "  --check               assemble and validate without writing outputs\n"
                      "  --no-tas              skip the .tas, for image-only builds\n"
                      "  --run-tests <dir>     assemble and simulate every .s test in the directory\n"
                      "  --diff                run the RV32I and REBEL-6 simulators and compare registers\n"
                      "  --stats <path>        write JSON telemetry for the assembled image\n"
                      "  --simulate            run the REBEL-6 simulator before writing stats\n"
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

void writeStats(const std::string& path, const Assemblers::REBEL6::ExecutableProgram& program,
                std::uint32_t bss, bool simulate)
{
   std::map<std::string, std::uint32_t> opcodes;
   const std::uint32_t count = program.getInstructionCount();
   std::uint8_t size = 0;
   for(std::uint32_t pc = 0; pc < count; pc += size)
   {
      const Expressions::Instruction* instr = program.loadInstruction(static_cast<std::int32_t>(pc), size);
      if(instr == nullptr)
      {
         break;
      }
      ++opcodes[instr->getInstructionName()];
   }

   std::uint32_t retired = 0;
   std::uint32_t flips = 0;
   bool passed = false;
   bool simulated = false;
   if(simulate)
   {
      Simulators::REBEL6::Simulator simulator;
      try
      {
         simulator.run(const_cast<Assemblers::REBEL6::ExecutableProgram&>(program));
      }
      catch(const Simulators::REBEL6::TestResult& result)
      {
         passed = result.passed;
      }
      retired = simulator.getNumberOfRanInstructions();
      flips = simulator.getNumberOfTritFlips();
      simulated = true;
   }

   std::ofstream file(path);
   if(!file)
   {
      throw RebelError("cannot write " + path);
   }
   file << "{\n";
   file << "  \"instructions\": " << count << ",\n";
   file << "  \"data\": " << program.getMemory().size() << ",\n";
   file << "  \"bss\": " << bss << ",\n";
   file << "  \"entry\": " << program.loadSymbolAddress("main") << ",\n";
   file << "  \"opcodes\": {";
   bool first = true;
   for(const auto& [name, occurrences]: opcodes)
   {
      if(!first)
      {
         file << ", ";
      }
      file << "\"" << name << "\": " << occurrences;
      first = false;
   }
   file << "},\n";
   if(simulated)
   {
      file << "  \"simulation\": {\"retired\": " << retired << ", \"trit_flips\": " << flips
           << ", \"passed\": " << (passed ? "true" : "false") << "}\n";
   }
   else
   {
      file << "  \"simulation\": null\n";
   }
   file << "}\n";
}

AssemblyResult assemble(Expressions::ExpressionList& expressions, const std::string& base, bool writeTas,
                        std::uint32_t memory, bool writeImage, const std::string& statsPath, bool simulate)
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

   if(writeImage)
   {
      std::int64_t entry = 0;
      try
      {
         entry = program.loadSymbolAddress("main");
      }
      catch(const std::exception&)
      {
         try
         {
            entry = program.loadSymbolAddress("_start");
         }
         catch(const std::exception&)
         {
            entry = 0;
         }
      }
      std::string error;
      if(!Encoder::writeImage(base + ".img", program, entry, error))
      {
         throw RebelError(error);
      }
   }

   if(!statsPath.empty())
   {
      writeStats(statsPath, program, assembler.getBssSizeTrytes(), simulate);
   }

   AssemblyResult result;
   result.symbols = program.getSymbols();
   result.bss = assembler.getBssSizeTrytes();
   return result;
}

int diffRun(const Options& options)
{
   const std::string source = readFile(options.inputs.front());
   Lexer lexer(source);
   Parser parser(&lexer);
   Expressions::ExpressionList parsed;
   parser.parse(parsed);

   Expressions::ExpressionList rv32iExpressions;
   Expressions::ExpressionList rebel6Expressions;
   Translators::Translator::Translate("rv32i", parsed, rv32iExpressions);
   Translators::Translator::Translate("rebel-6", parsed, rebel6Expressions);
   for(Expressions::Expression* expr: parsed)
   {
      delete expr;
   }

   Assemblers::RV32I::ExecutableProgram rv32iProgram;
   Assemblers::RV32I::Assembler rv32iAssembler(rv32iExpressions, rv32iProgram);
   rv32iAssembler.init();
   rv32iAssembler.run();

   Assemblers::REBEL6::ExecutableProgram rebel6Program;
   Assemblers::REBEL6::Assembler rebel6Assembler(rebel6Expressions, rebel6Program);
   rebel6Assembler.init();
   rebel6Assembler.run();

   Simulators::RV32I::Simulator rv32iSimulator;
   Simulators::REBEL6::Simulator rebel6Simulator;
   rv32iSimulator.run(rv32iProgram);
   rebel6Simulator.run(rebel6Program);

   for(Expressions::Expression* expr: rv32iExpressions)
   {
      delete expr;
   }
   for(Expressions::Expression* expr: rebel6Expressions)
   {
      delete expr;
   }

   int mismatches = 0;
   for(const std::string& name: rv32iSimulator.getRegisterNames())
   {
      if((name == "sp") || (name == "ra") || (name == "zero"))
      {
         continue;
      }
      const std::int32_t left = rv32iSimulator.getRegister(name);
      const std::int32_t right = static_cast<std::int32_t>(rebel6Simulator.getRegister(name));
      if(left != right)
      {
         std::cout << "MISMATCH " << name << ": rv32i=" << left << " rebel6=" << right << std::endl;
         ++mismatches;
      }
   }
   std::cout << "diff: " << mismatches << " mismatched registers, rv32i "
             << rv32iSimulator.getNumberOfRanInstructions() << " instructions, rebel6 "
             << rebel6Simulator.getNumberOfRanInstructions() << std::endl;
   return (mismatches == 0) ? 0 : 1;
}

int runTests(const std::string& directory)
{
   std::vector<std::filesystem::path> tests;
   for(const auto& entry: std::filesystem::directory_iterator(directory))
   {
      if(entry.is_regular_file() && (entry.path().extension() == ".s"))
      {
         tests.push_back(entry.path());
      }
   }
   std::sort(tests.begin(), tests.end());

   int passed = 0;
   int failed = 0;
   for(const std::filesystem::path& path: tests)
   {
      const std::string name = path.stem().string();
      try
      {
         const std::string source = readFile(path.string());
         Lexer lexer(source);
         Parser parser(&lexer);
         Expressions::ExpressionList parsed;
         parser.parse(parsed);
         Expressions::ExpressionList translated;
         Translators::Translator::Translate("rebel-6", parsed, translated);
         for(Expressions::Expression* expr: parsed)
         {
            delete expr;
         }

         Assemblers::REBEL6::ExecutableProgram program;
         Assemblers::REBEL6::Assembler assembler(translated, program);
         assembler.init();
         assembler.run();

         Simulators::REBEL6::Simulator simulator;
         simulator.run(program);
         for(Expressions::Expression* expr: translated)
         {
            delete expr;
         }
         std::cout << "FAIL " << name << " (no ecall result)" << std::endl;
         ++failed;
      }
      catch(const Simulators::REBEL6::TestResult& result)
      {
         if(result.passed)
         {
            std::cout << "PASS " << name << std::endl;
            ++passed;
         }
         else
         {
            std::cout << "FAIL " << name << " (test " << result.test << ")" << std::endl;
            ++failed;
         }
      }
      catch(const std::exception& error)
      {
         std::cout << "FAIL " << name << " (" << error.what() << ")" << std::endl;
         ++failed;
      }
   }

   std::cout << "tests: " << passed << "/" << (passed + failed) << " passed" << std::endl;
   return (failed == 0) ? 0 : 1;
}

int linkUnits(const Options& options)
{
   std::vector<Linker::Unit> units;
   for(const std::string& input: options.inputs)
   {
      const std::string source = readFile(input);
      const std::string stem = stemOf(input);
      if(input.size() > 4 && input.compare(input.size() - 4, 4, ".mto") == 0)
      {
         units.push_back(Linker::Unit::fromObject(stem, source));
      }
      else
      {
         units.push_back(Linker::Unit::fromAssembly(stem, source));
      }
      units.back().setPath(input);
   }

   if(!options.mapPath.empty())
   {
      Linker::writeMap(options.mapPath, units);
   }
   if(!options.symsPath.empty())
   {
      Linker::writeSymbols(options.symsPath, units);
   }

   Expressions::ExpressionList merged = Linker::link(units);
   const std::string base = options.output.empty() ? "linked" : options.output;
   assemble(merged, base, !options.check && !options.noTas, options.memory, options.emitImage && !options.check,
            options.statsPath, options.simulate);
   for(Expressions::Expression* expr: merged)
   {
      delete expr;
   }
   if(options.check)
   {
      std::cout << "check: ok" << std::endl;
   }
   return 0;
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
      assemble(expressions, options.output.empty() ? stem : options.output, !options.check && !options.noTas, options.memory,
               options.emitImage && !options.check, options.statsPath, options.simulate);
      for(Expressions::Expression* expr: expressions)
      {
         delete expr;
      }
      if(options.check)
      {
         std::cout << "check: ok" << std::endl;
      }
      return 0;
   }

   if(options.emitObject)
   {
      Linker::Unit unit = Linker::Unit::fromAssembly(stem, source);
      const std::vector<std::string> relocations = collectRelocations(unit);
      AssemblyResult result = assemble(unit.getExpressions(), stem, false, options.memory, false,
                                       options.statsPath, options.simulate);
      unit.writeObject(stem + ".mto", result.symbols, relocations, result.bss);
      for(Expressions::Expression* expr: unit.getExpressions())
      {
         delete expr;
      }
      return 0;
   }

   // Plain single-file path: translate without unit scoping, exactly as before.
   Lexer lexer(source);
   Parser parser(&lexer);
   Expressions::ExpressionList parsed;
   parser.parse(parsed);
   Linker::applyDebugDirectives(parsed);

   Expressions::ExpressionList translated;
   Translators::Translator::Translate(options.target, parsed, translated);
   for(Expressions::Expression* expr: parsed)
   {
      delete expr;
   }

   assemble(translated, options.output.empty() ? stem : options.output, !options.check && !options.noTas, options.memory,
            options.emitImage && !options.check, options.statsPath, options.simulate);
   for(Expressions::Expression* expr: translated)
   {
      delete expr;
   }
   if(options.check)
   {
      std::cout << "check: ok" << std::endl;
   }
   return 0;
}

int run(int argc, char* argv[])
{
   const Options options = parseOptions(argc, argv);
   if(!options.testDir.empty())
   {
      return runTests(options.testDir);
   }
   if(options.diff)
   {
      if(options.inputs.size() != 1)
      {
         throw RebelError("--diff takes exactly one input");
      }
      return diffRun(options);
   }
   if(options.inputs.empty())
   {
      std::cerr << "usage: RV32IToREBEL [options] <file.s|file.mto>...\n";
      return 2;
   }
   if(options.link || (options.inputs.size() > 1) || !options.mapPath.empty() || !options.symsPath.empty())
   {
      return linkUnits(options);
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
