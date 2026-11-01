#pragma once

#include <Linker/unit.h>

#include <string>
#include <vector>

namespace Linker
{
// Merges the units into one expression list and rejects duplicate global definitions.
// The units are left empty: ownership of their expressions moves into the result.
Expressions::ExpressionList link(std::vector<Unit>& units);

// Consumes `.file` and `.loc`, annotating every expression with its C origin, then removes them.
void applyDebugDirectives(Expressions::ExpressionList& expressions);

// Writes, per unit, the assembly line and the C file and line in effect on it.
void writeMap(const std::string& path, const std::vector<Unit>& units);

// Writes every symbol with its scope, unit, C line and original name.
void writeSymbols(const std::string& path, const std::vector<Unit>& units);
}
