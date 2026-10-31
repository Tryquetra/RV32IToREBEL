#pragma once

#include <sstream>
#include <stdexcept>
#include <string>

// Position of a token or expression in the input file. line 0 means unknown.
struct SourceLocation
{
   int line = 0;
   int column = 0;
};

// Error raised by the front end and the assembler for invalid input. main() reports the message
// on stderr and exits with code 2, so a bad input never aborts the process or produces a core.
class RebelError : public std::runtime_error
{
public:
   explicit RebelError(const std::string& message, SourceLocation location = {})
      : std::runtime_error(format(message, location))
   {
   }

private:
   static std::string format(const std::string& message, SourceLocation location)
   {
      if(location.line <= 0)
      {
         return message;
      }
      return "line " + std::to_string(location.line) + ":" + std::to_string(location.column) + ": " + message;
   }
};

// Streams any value into a string, for error messages that embed expressions and numbers.
template <typename T>
std::string toText(const T& value)
{
   std::ostringstream stream;
   stream << value;
   return stream.str();
}
