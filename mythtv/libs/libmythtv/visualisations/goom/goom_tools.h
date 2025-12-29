#ifndef GOOMTOOLS_H
#define GOOMTOOLS_H

#include <cinttypes>
#include <cstddef>
#include <cstdlib>

#if !defined( M_PI ) 
static constexpr double M_PI    {  3.14159265358979323846  };
#endif
static constexpr float  M_PI_F  { static_cast<float>(M_PI) };

#endif // GOOMTOOLS_H
