#ifndef MYTHUICOLOURSPACE_H
#define MYTHUICOLOURSPACE_H

// Std
#include <array>

// Qt
#include <QMatrix4x4>

// MythTV
#include "mythuiexp.h"

using MythPrimaryFloat    = std::array<float,2>;;
using MythPrimariesFloat  = std::array<MythPrimaryFloat,3>;
using MythPrimaryUInt16   = std::array<uint16_t,2>;;
using MythPrimariesUInt16 = std::array<MythPrimaryUInt16,3>;

class MUI_PUBLIC MythColourSpace
{
  public:
    static MythColourSpace s_sRGB;
    static MythColourSpace s_BT709;
    static MythColourSpace s_BT470M;
    static MythColourSpace s_BT610_525;
    static MythColourSpace s_BT610_625;
    static MythColourSpace s_BT2020;

    static bool Alike(const MythColourSpace& First, const MythColourSpace& Second, float Fuzz);
    static QMatrix4x4 RGBtoXYZ(const MythColourSpace& Primaries);

    MythColourSpace() = default;
    MythColourSpace(const MythPrimariesFloat& Primaries, MythPrimaryFloat WhitePoint);

    MythPrimariesFloat m_primaries  {{{0.0F}}};
    MythPrimaryFloat   m_whitePoint   {0.0F};
};


#endif
