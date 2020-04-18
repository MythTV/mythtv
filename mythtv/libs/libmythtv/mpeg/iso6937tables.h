#include <array>

using uint16_t = unsigned short;
using iso6937table = std::array<const uint16_t,256>;

// conversion tables from iso/iec 6937 to unicode

extern const iso6937table iso6937table_base;
extern const std::array<const iso6937table *,256> iso6937table_secondary;
