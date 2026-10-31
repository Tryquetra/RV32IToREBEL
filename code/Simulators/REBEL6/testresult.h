#pragma once

#include <exception>

namespace Simulators
{
namespace REBEL6
{
// Thrown by the ecall handler when a RISC-V test reports its result.
struct TestResult : std::exception
{
   bool passed;
   int test;

   TestResult(bool passed, int test)
      : passed(passed)
      , test(test)
   {
   }

   const char* what() const noexcept override
   {
      return passed ? "test passed" : "test failed";
   }
};
}
}
