#ifndef GOOMCORE_H
#define GOOMCORE_H

#include <array>
#include <cstdint>

#include "libmythtv/mythtvexp.h"

#define NB_FX 10

using GoomSingleData = std::array<int16_t,512>;
using GoomDualData   = std::array<GoomSingleData,2>;

MTV_PUBLIC void goom_init (uint32_t resx, uint32_t resy, int cinemascope);
MTV_PUBLIC void goom_set_resolution (uint32_t resx, uint32_t resy, int cinemascope);

/*
 * forceMode == 0 : do nothing
 * forceMode == -1 : lock the FX
 * forceMode == 1..NB_FX : force a switch to FX n°forceMode
 */
MTV_PUBLIC uint32_t *goom_update (GoomDualData& data, int forceMode);
MTV_PUBLIC void    goom_close (void);
MTV_PUBLIC uint32_t goom_rand (void);

#endif // GOOMCORE_H
