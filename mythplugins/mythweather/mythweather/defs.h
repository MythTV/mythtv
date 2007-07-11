
#ifndef _WEATHER_DEFS_H_
#define _WEATHER_DEFS_H_
#include <qmap.h>
#include <qstring.h>
#include <iostream>
typedef unsigned char units_t;

#define SI_UNITS 0
#define ENG_UNITS 1


#define DEFAULT_UPDATE_TIMEOUT (5*60*1000)
#define DEFAULT_SCRIPT_TIMEOUT (60*1000)

/* because we like clean builds, warning messages are annoying */
#if __GNUC__ >= 3
#define __unused __attribute__ ((unused))
#else
#define __unused /* no unused attribute */
#endif

typedef QMap<QString,QString> DataMap;

//typedef QValueList::iterator screen_iterator;
#endif

