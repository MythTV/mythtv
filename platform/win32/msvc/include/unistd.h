#pragma once

#include "compat.h"
#include "stdint.h"

__inline unsigned int usleep( unsigned int us )
{
    Sleep( (us + 999) / 1000 );
    return 0;
}

__inline int close( int fd )
{
    return _close( fd );
}

__inline int write( int fd, const void *buffer, unsigned int count )
{
    return _write( fd, buffer, count );
}

// This doesn't belong here, but helps minimize changes to xine_demux_sputext.cpp

#define strncasecmp _strnicmp
#define strcasecmp  _stricmp

typedef __int64 off64_t;
typedef int		pid_t;
                                
#ifndef MAX
#define MAX( a, b )  ((a>b)?a:b)
#endif

#ifndef MIN
#define MIN( a, b )  ((a<b)?a:b)
#endif
