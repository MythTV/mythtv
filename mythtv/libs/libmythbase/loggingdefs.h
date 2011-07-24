#ifndef LOGGINGDEFS_H_
#define LOGGINGDEFS_H_

#include "mythbaseexp.h"  //  MBASE_PUBLIC , etc.

typedef enum
{
    LOG_ANY = -1,       // For use with masking, not actual logging
    LOG_EMERG = 0,
    LOG_ALERT,
    LOG_CRIT,
    LOG_ERR,
    LOG_WARNING,
    LOG_NOTICE,
    LOG_INFO,
    LOG_DEBUG,
    LOG_UNKNOWN
} LogLevel_t;

extern MBASE_PUBLIC const char *LogLevelNames[];
extern MBASE_PUBLIC int LogLevelNameCount;
extern MBASE_PUBLIC const char LogLevelShortNames[];
extern MBASE_PUBLIC int LogLevelShortNameCount;

#ifdef _LogLevelNames_
const char *LogLevelNames[] =
{
    "LOG_EMERG",
    "LOG_ALERT",
    "LOG_CRIT",
    "LOG_ERR",
    "LOG_WARNING",
    "LOG_NOTICE",
    "LOG_INFO",
    "LOG_DEBUG",
    "LOG_UNKNOWN"
};
int LogLevelNameCount = sizeof(LogLevelNames) / sizeof(LogLevelNames[0]);

const char LogLevelShortNames[] =
{
    '!',
    'A',
    'C',
    'E',
    'W',
    'N',
    'I',
    'D',
    '-'
};
int LogLevelShortNameCount = sizeof(LogLevelShortNames) / 
                             sizeof(LogLevelShortNames[0]);
#endif

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
