#include "Translators/translator.h"

#include "Parsers/lexer.h"
#include "Parsers/token.h"
#include "Parsers/parser.h"

#include "Expressions/expression.h"
#include "Expressions/all_expressions.h"

#include "Assemblers/RV32I/assembler.h"
#include "Assemblers/RV32I/assemblerutils.h"
#include "Assemblers/RV32I/executableprogram.h"
#include "Simulators/RV32I/simulator.h"

#include "Assemblers/REBEL6/assembler.h"
#include "Assemblers/REBEL6/executableprogram.h"
#include "Simulators/REBEL6/simulator.h"

#include <iostream>
#include <fstream>
#include <filesystem>

#include <errors.h>


int run(int argc, char* argv[]);

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

int run(int argc, char* argv[])
{
   if(argc != 2)
   {
      std::cerr << "usage: RV32IToREBEL <file.s>" << std::endl;
      return 2;
   }

   std::string inputPath = argv[1];

   std::ifstream inFile(inputPath);
   if(!inFile)
   {
      throw RebelError("cannot open " + inputPath);
   }
   std::string source((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
   source += '\n';

   /*
    * ------------------ FRONTEND ------------------
    */

   Lexer* l = new Lexer(source);
   Parser p(l);
   Expressions::ExpressionList sourceExpressions;
   p.parse(sourceExpressions);
   std::cout << "Parsed successfully." << std::endl;

   /*
    * --------- INTERMEDIATE REPRESENTAITON ---------
    */
   // NA - The RV32I input is currently considered the IR code.


   /*
    * ------------------- BACKEND -------------------
    */

   // ------------ REBEL-6 ------------

   // Translation
   Expressions::ExpressionList rebel6Expressions;
   Translators::Translator::Translate("rebel-6", sourceExpressions, rebel6Expressions);

   // Assembling
   Assemblers::REBEL6::ExecutableProgram rebel6Executable;
   Assemblers::REBEL6::Assembler rebel6Assembler(rebel6Expressions, rebel6Executable);

   std::string fileName = std::filesystem::path(inputPath).stem();

   // Generate ternary assembly file
   rebel6Assembler.printExpressionsToFile(fileName);

   // Run assembler
   rebel6Assembler.init();
   rebel6Assembler.run();

   return 0;
}
