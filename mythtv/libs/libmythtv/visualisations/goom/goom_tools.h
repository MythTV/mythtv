#ifndef GOOMTOOLS_H
#define GOOMTOOLS_H

#include <cinttypes>
#include <cstddef>
#include <cstdlib>
#include <numbers>

#if !defined( M_PI ) 
static constexpr double M_PI    { std::numbers::pi };
#endif
static constexpr float  M_PI_F  { std::numbers::pi_v<float> };

#endif // GOOMTOOLS_H
