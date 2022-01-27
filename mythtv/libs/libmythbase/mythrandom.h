#ifndef MYTH_RANDOM_H_
#define MYTH_RANDOM_H_
/**
@file
Convenience inline random number generator functions
*/

#include <cstdint>
//#include <random> // could be used instead of Qt

#if QT_VERSION < QT_VERSION_CHECK(5,10,0)
#include <QtGlobal>
#else
#include <QRandomGenerator>
#endif

#include "mythbaseexp.h"

/**
@brief generate 32 random bits
*/
inline uint32_t MythRandom()
{
#if QT_VERSION < QT_VERSION_CHECK(5,10,0)
    return static_cast<uint32_t>(qrand());
#else
    return QRandomGenerator::global()->generate();
#endif
}

#if QT_VERSION >= QT_VERSION_CHECK(5,10,0)
/**
@brief generate 64 random bits
*/
inline uint64_t MythRandom64()
{
    return QRandomGenerator::global()->generate64();
}

/**
@brief generate a random uint32_t in the range [min, max]
*/
inline uint32_t MythRandom(uint32_t min, uint32_t max)
{
    return QRandomGenerator::global()->bounded(min, max);
}

/**
@brief generate a random signed int in the range [min, max]
*/
inline int MythRandom(int min, int max)
{
    return QRandomGenerator::global()->bounded(min, max);
}

#endif


#endif // MYTH_RANDOM_H_
