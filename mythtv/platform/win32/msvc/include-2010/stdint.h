#pragma once

#include <math.h>
#include <float.h>
//#include <sys\time.h>

#define INT8_MIN         (-127i8 - 1)
#define INT16_MIN        (-32767i16 - 1)
#define INT32_MIN        (-2147483647i32 - 1)
#define INT64_MIN        (-9223372036854775807i64 - 1)
#define INT8_MAX         127i8
#define INT16_MAX        32767i16
#define INT32_MAX        2147483647i32
#define INT64_MAX        9223372036854775807i64
#define UINT8_MAX        0xffui8
#define UINT16_MAX       0xffffui16
#define UINT32_MAX       0xffffffffui32
#define UINT64_MAX       0xffffffffffffffffui64


//#define ssize_t  __int64

typedef __int64             int64_t;    // Define it from MSVC's internal type
typedef __int32             int32_t;    
typedef __int16             int16_t;    
typedef __int8              int8_t;    

typedef unsigned __int64    uint64_t;
typedef unsigned __int32    uint32_t;
typedef unsigned __int16    uint16_t;
typedef unsigned __int8     uint8_t;

typedef int                 pid_t;
typedef int                 uid_t;

typedef uint32_t            mode_t;

#define isnan( x )          _isnan( x )

#ifndef INT64_C
# define INT64_C(x) x##i64
#endif

#ifndef UINT64_C
# define UINT64_C(x) x##ui64
#endif
/*
#ifndef INT64_MAX
#define INT64_MAX INT64_C(9223372036854775807)
#endif
*/
#ifdef __STDC_LIMIT_MACROS  
# define INT8_MAX 0x7f 
# define INT8_MIN (-INT8_MAX - 1) 
# define UINT8_MAX (__CONCAT(INT8_MAX, U) * 2U + 1U) 
# define INT16_MAX 0x7fff 
# define INT16_MIN (-INT16_MAX - 1) 
# define UINT16_MAX (__CONCAT(INT16_MAX, U) * 2U + 1U) 
# define INT32_MAX 0x7fffffffL 
# define INT32_MIN (-INT32_MAX - 1L) 
# define UINT32_MAX (__CONCAT(INT32_MAX, U) * 2UL + 1UL) 
# define INT64_MAX 0x7fffffffffffffffLL 
# define INT64_MIN (-INT64_MAX - 1LL) 
# define UINT64_MAX (__CONCAT(INT64_MAX, U) * 2ULL + 1ULL) 
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

