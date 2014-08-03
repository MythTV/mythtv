
#ifndef __COMPAT_TIME_H__
#define __COMPAT_TIME_H__

#include "compat.h"

#include <time.h>
#include <sys/utime.h>
#include <winsock2.h>	// for timeval

//typedef struct timeval {
//	long tv_sec;
//	long tv_usec;
//} timeval;

#if defined(_MSC_VER) || defined(__BORLANDC__)
#define EPOCHFILETIME (116444736000000000i64)
#else
#define EPOCHFILETIME (116444736000000000LL)
#endif

__inline int gettimeofday( struct timeval *tv, void *tz )                   
{                                                
    FILETIME        ft;                          
    LARGE_INTEGER   li;                          
    __int64         t;                           
    static int      tzflag;                      
                                                 
    if (tv)                                      
    {                                            
        GetSystemTimeAsFileTime(&ft);            
        li.LowPart  = ft.dwLowDateTime;          
        li.HighPart = ft.dwHighDateTime;         
        t  = li.QuadPart;                        
        t -= EPOCHFILETIME;                      
        t /= 10;                                 
        (tv)->tv_sec  = (long)(t / 1000000);     
        (tv)->tv_usec = (long)(t % 1000000);     
    }     

    return 0;
}

#define utimbuf       _utimbuf
#define utime( a, b ) _utime( a, b )

struct timezone
{
    int tz_dsttime;
    int tz_minuteswest;
};


#endif 
