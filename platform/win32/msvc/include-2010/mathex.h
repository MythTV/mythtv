#ifndef __MATHEX_H
#define __MATHEX_H

#include <math.h>

// Methods missing from Visual Studio 2010 Math.h

#ifndef isnan
#define isnan( x )  _isnan( x )
#endif

__inline double roundf(double x)
{
    return floor(x + 0.5);
}

__inline int truncf( float flt )
{
    return (int)floor( flt );
}

__inline long int lrint (double flt)
{
    int intgr;
    _asm
    {
        fld flt
        fistp intgr
    };

    return intgr;
}

__inline long int lrintf (float flt)
{
    int intgr;
    _asm
    {
        fld flt
        fistp intgr
    };

    return intgr;
}

#endif
