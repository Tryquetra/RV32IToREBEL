#pragma once

#include <string>
#include <unordered_map>

namespace Encoder
{
struct Spec
{
   char format;
   int opcode;
   int func;
};

inline const std::unordered_map<std::string, Spec>& specs()
{
   static const std::unordered_map<std::string, Spec> table = {
      {"add.t", {'R', -36, -4}},
      {"sub.t", {'R', -36, -3}},
      {"sh.t", {'R', -36, -2}},
      {"rotr.t", {'R', -36, -1}},
      {"slt.t", {'R', -36, 0}},
      {"or.t", {'R', -36, 1}},
      {"xor.t", {'R', -36, 2}},
      {"and.t", {'R', -36, 3}},
      {"cmp.t", {'R', -27, -4}},
      {"sti.t", {'R', -27, -3}},
      {"mv2.t", {'R', -27, -2}},
      {"min.t", {'R', -27, -1}},
      {"max.t", {'R', -27, 1}},
      {"addi.t", {'I', 0, 0}},
      {"shi.t", {'I', 0, -4}},
      {"rot.t", {'I', 0, -2}},
      {"slti.t", {'I', 0, -1}},
      {"ori.t", {'I', 0, 1}},
      {"xori.t", {'I', 0, 2}},
      {"andi.t", {'I', 0, 3}},
      {"lw.t", {'I', -18, -4}},
      {"lh.t", {'I', -18, -3}},
      {"lt.t", {'I', -18, -2}},
      {"jalr.t", {'I', -18, -1}},
      {"bcgs.t", {'B', -9, -4}},
      {"bceg.t", {'B', -9, -3}},
      {"beq.t", {'B', -9, -2}},
      {"bne.t", {'B', -9, -1}},
      {"blt.t", {'B', -9, 0}},
      {"bge.t", {'B', -9, 1}},
      {"sw.t", {'B', 9, -4}},
      {"sh.t", {'B', 9, -3}},
      {"st.t", {'B', 9, -2}},
      {"majv.t", {'D', 18, -4}},
      {"minv.t", {'D', 18, -3}},
      {"li2.t", {'X', 27, -4}},
      {"fence.t", {'R', 36, -3}},
      {"wfi.t", {'R', 36, -2}},
      {"tret.t", {'R', 36, 2}},
      {"ebreak.t", {'R', 36, 3}},
      {"ecall.t", {'R', 36, 4}},
      {"lwa.t", {'G', 4, 0}},
      {"li.t", {'G', 1, 0}},
      {"swa.t", {'Y', -2, 0}},
      {"jal.t", {'G', 2, 0}},
      {"aipc.t", {'G', -1, 0}},
      {"add", {'R', -39, -4}},
      {"sub", {'R', -39, -3}},
      {"sll", {'R', -39, -2}},
      {"srl", {'R', -39, -1}},
      {"sra", {'R', -39, 0}},
      {"sltu", {'R', -39, 1}},
      {"or", {'R', -39, 2}},
      {"xor", {'R', -39, 3}},
      {"and", {'R', -39, 4}},
      {"addi", {'I', -3, -4}},
      {"slli", {'I', -3, -3}},
      {"srli", {'I', -3, -2}},
      {"srai", {'I', -3, -1}},
      {"sltiu", {'I', -3, 0}},
      {"ori", {'I', -3, 1}},
      {"xori", {'I', -3, 2}},
      {"andi", {'I', -3, 3}},
      {"lw", {'I', -21, -4}},
      {"lh", {'I', -21, -3}},
      {"lb", {'I', -21, -2}},
      {"lhu", {'I', -21, -1}},
      {"lbu", {'I', -21, 0}},
      {"bltu", {'B', -12, -4}},
      {"bgeu", {'B', -12, -3}},
      {"sw", {'B', 6, -4}},
      {"sh", {'B', 6, -3}},
      {"sb", {'B', 6, -2}},
   };
   return table;
}
}
