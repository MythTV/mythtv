#ifndef __MYTHCHRONO_H__
#define __MYTHCHRONO_H__

#include <cmath>
#include <sys/time.h> // For gettimeofday

// Include the std::chrono definitions, and set up to parse the
// std::chrono literal suffixes.
#include <chrono>
using std::chrono::duration_cast;
using namespace std::chrono_literals;

#endif // __MYTHCHRONO_H__
