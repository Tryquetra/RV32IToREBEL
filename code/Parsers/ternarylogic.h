#pragma once

#include <cstdint>
#include <vector>

using Trit = std::int8_t;   // Can be -1, 0, 1
using Tryte = std::int16_t; // Can be between -+ (3^6 - 1) / 2
using Tint = std::int64_t;  // Can be between -+ (3^24 - 1) / 2
using Trits = std::vector<Trit>;
using Trytes = std::vector<Tryte>;

// Powers of three used by the ternary conversions. 3^31 still fits in int64.
constexpr std::int64_t POW3[32] = {
   1LL, 3LL, 9LL, 27LL, 81LL, 243LL, 729LL, 2187LL, 6561LL, 19683LL, 59049LL,
   177147LL, 531441LL, 1594323LL, 4782969LL, 14348907LL, 43046721LL, 129140163LL,
   387420489LL, 1162261467LL, 3486784401LL, 10460353203LL, 31381059609LL,
   94143178827LL, 282429536481LL, 847288609443LL, 2541865828329LL, 7625597484987LL,
   22876792454961LL, 68630377364883LL, 205891132094649LL, 617673396283947LL,
};

class TernaryLogic
{
public:
  TernaryLogic() = delete;

  static void tintToTrits(Tint in, Trits &out);
  static void tritsToTint(const Trits &in, Tint &out);

  // Truncates to the field width and returns false when the value did not fit.
  static bool parseImmediate(std::uint8_t immediateSizeTrits, const Tint &in, Tint &out);
};
