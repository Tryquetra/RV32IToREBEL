#pragma once

#include <Linker/unit.h>

#include <string>
#include <vector>

namespace Linker
{
// Merges the units into one expression list and rejects duplicate global definitions.
// The units are left empty: ownership of their expressions moves into the result.
Expressions::ExpressionList link(std::vector<Unit>& units);
}
