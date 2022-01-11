#ifndef MYTH_RANDOM_H_
#define MYTH_RANDOM_H_
/**
@file
Convenience inline random number generator functions
*/

#include <cstdint>
#include <random>

inline namespace MythRandomStd
{

using MythRandomGenerator_32 = std::mt19937;
using MythRandomGenerator_64 = std::mt19937_64;

/**
@brief generate 32 random bits
*/
inline uint32_t MythRandom()
{
    static std::random_device rd;
    static MythRandomGenerator_32 generator(rd());
    return generator();

}

/**
@brief generate 64 random bits
*/
inline uint64_t MythRandom64()
{
    static std::random_device rd;
    static MythRandomGenerator_64 generator(rd());
    return generator();
}

/**
@brief generate a uniformly distributed random uint32_t in the closed interval [min, max].

The behavior is undefined if \f$min > max\f$.

An alternate name would be MythRandomU32.
*/
inline uint32_t MythRandom(uint32_t min, uint32_t max)
{
    static std::random_device rd;
    static MythRandomGenerator_32 generator(rd());
    std::uniform_int_distribution<uint32_t> distrib(min, max);
    return distrib(generator);
}

/**
@brief generate a uniformly distributed random signed int in the closed interval [min, max].

The behavior is undefined if \f$min > max\f$.
*/
inline int MythRandomInt(int min, int max)
{
    static std::random_device rd;
    static MythRandomGenerator_32 generator(rd());
    std::uniform_int_distribution<int> distrib(min, max);
    return distrib(generator);
}

/**
@brief return a random bool with P(true) = 1/chance

An input less than 2 always returns true:
 - for chance = 1: range is [0, 0], i.e. always 0
 - for chance = 0: range is [0, -1], unsigned integer underflow! (or just undefined behavior if signed)

This is a Bernoulli distribution with \f$p = 1 / chance\f$.
*/
inline bool rand_bool(uint32_t chance = 2)
{
    if (chance < 2)
    {
        return true;
    }
    return MythRandom(0U, chance - 1) == 0U;
}

} // namespace MythRandomStd

#endif // MYTH_RANDOM_H_
