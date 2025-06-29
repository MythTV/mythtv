// MythTV
#include "mythcolourspace.h"

// Qt
#include <QtGlobal>

#define D65 {0.31271F, 0.32902F}
#define Cwp {0.31006F, 0.31616F}

MythColourSpace MythColourSpace::s_sRGB      = {{{{0.640F, 0.330F}, {0.300F, 0.600F}, {0.150F, 0.060F}}}, D65};
MythColourSpace MythColourSpace::s_BT709     = {{{{0.640F, 0.330F}, {0.300F, 0.600F}, {0.150F, 0.060F}}}, D65};
MythColourSpace MythColourSpace::s_BT610_525 = {{{{0.630F, 0.340F}, {0.310F, 0.595F}, {0.155F, 0.070F}}}, D65};
MythColourSpace MythColourSpace::s_BT610_625 = {{{{0.640F, 0.330F}, {0.290F, 0.600F}, {0.150F, 0.060F}}}, D65};
MythColourSpace MythColourSpace::s_BT2020    = {{{{0.708F, 0.292F}, {0.170F, 0.797F}, {0.131F, 0.046F}}}, D65};

// This is currently unused because I have no material that uses it and hence cannot test it.
// As it uses a different white point it will need an additional 'Chromatic adaption'
// stage in the primaries conversion
MythColourSpace MythColourSpace::s_BT470M    = {{{{0.670F, 0.330F}, {0.210F, 0.710F}, {0.140F, 0.080F}}}, Cwp};

MythColourSpace::MythColourSpace(const MythPrimariesFloat& Primaries, MythPrimaryFloat WhitePoint)
  : m_primaries(Primaries),
    m_whitePoint(WhitePoint)
{
}

bool MythColourSpace::Alike(const MythColourSpace &First, const MythColourSpace &Second, float Fuzz)
{
    auto cmp = [=](float One, float Two) { return (qAbs(One - Two) < Fuzz); };
    return cmp(First.m_primaries[0][0], Second.m_primaries[0][0]) &&
           cmp(First.m_primaries[0][1], Second.m_primaries[0][1]) &&
           cmp(First.m_primaries[1][0], Second.m_primaries[1][0]) &&
           cmp(First.m_primaries[1][1], Second.m_primaries[1][1]) &&
           cmp(First.m_primaries[2][0], Second.m_primaries[2][0]) &&
           cmp(First.m_primaries[2][1], Second.m_primaries[2][1]) &&
           cmp(First.m_whitePoint[0],   Second.m_whitePoint[0]) &&
           cmp(First.m_whitePoint[1],   Second.m_whitePoint[1]);
}

inline float CalcBy(const MythPrimariesFloat& p, const MythPrimaryFloat w)
{
    float val = (((1-w[0])/w[1] - (1-p[0][0])/p[0][1]) * (p[1][0]/p[1][1] - p[0][0]/p[0][1])) -
    ((w[0]/w[1] - p[0][0]/p[0][1]) * ((1-p[1][0])/p[1][1] - (1-p[0][0])/p[0][1]));
    val /= (((1-p[2][0])/p[2][1] - (1-p[0][0])/p[0][1]) * (p[1][0]/p[1][1] - p[0][0]/p[0][1])) -
    ((p[2][0]/p[2][1] - p[0][0]/p[0][1]) * ((1-p[1][0])/p[1][1] - (1-p[0][0])/p[0][1]));
    return val;
}

inline float CalcGy(const MythPrimariesFloat& p, const MythPrimaryFloat w, const float By)
{
    float val = (w[0]/w[1]) - (p[0][0]/p[0][1]) - (By * (p[2][0]/p[2][1] - p[0][0]/p[0][1]));
    val /= (p[1][0]/p[1][1]) - (p[0][0]/p[0][1]);
    return val;
}

/*! \brief Create a conversion matrix for RGB to XYZ with the given primaries
 *
 * This is a joyous mindbender. There are various explanations on the interweb
 * but this is based on the Kodi implementation - with due credit to Team Kodi.
 *
 * Invert the result for the XYZ->RGB conversion matrix.
 *
 * \note We use QMatrix4x4 because QMatrix3x3 has no inverted method.
 */
QMatrix4x4 MythColourSpace::RGBtoXYZ(const MythColourSpace& Primaries)
{
    float By = CalcBy(Primaries.m_primaries, Primaries.m_whitePoint);
    float Gy = CalcGy(Primaries.m_primaries, Primaries.m_whitePoint, By);
    float Ry = 1.0F - Gy - By;

    return {
        // Row 0
        Ry * Primaries.m_primaries[0][0] / Primaries.m_primaries[0][1],
        Gy * Primaries.m_primaries[1][0] / Primaries.m_primaries[1][1],
        By * Primaries.m_primaries[2][0] / Primaries.m_primaries[2][1],
        0.0F,
        // Row 1
        Ry, Gy, By, 0.0F,
        // Row 2
        Ry / Primaries.m_primaries[0][1] * (1 - Primaries.m_primaries[0][0] - Primaries.m_primaries[0][1]),
        Gy / Primaries.m_primaries[1][1] * (1 - Primaries.m_primaries[1][0] - Primaries.m_primaries[1][1]),
        By / Primaries.m_primaries[2][1] * (1 - Primaries.m_primaries[2][0] - Primaries.m_primaries[2][1]),
        0.0F,
        // Row 3
        0.0F, 0.0F, 0.0F, 1.0F };
}
