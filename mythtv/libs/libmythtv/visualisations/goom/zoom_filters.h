#include <array>
#include <vector>

using sintvec = std::vector<signed int>;
using GoomCoefficients = std::array<std::array<int,16>,16>;

void    zoom_filter_sse2(int prevX, int prevY,
                         const unsigned int *expix1, unsigned int *expix2,
                         const sintvec& brutS, const sintvec& brutD,
                         int buffratio, const GoomCoefficients &precalCoef);
