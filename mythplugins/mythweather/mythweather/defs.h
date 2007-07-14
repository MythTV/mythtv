#ifndef _WEATHER_DEFS_H_
#define _WEATHER_DEFS_H_

#include <qmap.h>
#include <qstring.h>

typedef unsigned char units_t;

#define SI_UNITS 0
#define ENG_UNITS 1


#define DEFAULT_UPDATE_TIMEOUT (5*60*1000)
#define DEFAULT_SCRIPT_TIMEOUT (60*1000)

typedef QMap<QString, QString> DataMap;

#endif
