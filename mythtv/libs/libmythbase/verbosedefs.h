#ifndef VERBOSEDEFS_H_
#define VERBOSEDEFS_H_

#ifdef __cplusplus
#include <QMap>
#include <QString>
#include <QMutex>
#include <cstdint>
#else
#include <stdint.h>
#endif

/// This file gets included in two different ways:
/// 1) from mythlogging.h from nearly every file.  This will define the
///    VerboseMask enum
/// 2) specifically (and a second include with MYTH_IMPLEMENT_VERBOSE defined)
///    from mythlogging.cpp.  This is done in verboseInit (in the middle of the
///    function) as it will expand out to a series of calls to verboseAdd() to
///    fill the verboseMap.
///
/// The 4 fields are:
///     enum name (expected to start with VB_)
///     enum value (will be used as a 64-bit unsigned int)
///     additive flag (explicit = false, additive = true)
///     help text for "-v help"
///
/// To create a new VB_* flag, this is the only piece of code you need to
/// modify, then you can start using the new flag and it will automatically be
/// processed by the verboseArgParse() function and help info printed when
/// "-v help" is used.

#undef VERBOSE_PREAMBLE
#undef VERBOSE_POSTAMBLE
#undef VERBOSE_MAP

#undef LOGLEVEL_PREAMBLE
#undef LOGLEVEL_POSTAMBLE
#undef LOGLEVEL_MAP

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#ifdef MYTH_IMPLEMENT_VERBOSE

// This is used to actually implement the mask in mythlogging.cpp
#define VERBOSE_PREAMBLE
#define VERBOSE_POSTAMBLE
#define VERBOSE_MAP(name,mask,additive,help) \
    verboseAdd(mask,QString(#name),additive,QString(help));

#define LOGLEVEL_PREAMBLE
#define LOGLEVEL_POSTAMBLE
#define LOGLEVEL_MAP(name,value,shortname) \
    loglevelAdd(value,QString(#name),shortname);

#else // !defined(MYTH_IMPLEMENT_VERBOSE)

// This is used to define the enumerated type (used by all files)

#ifndef _MSC_VER
    #define VERBOSE_PREAMBLE \
        enum VerboseMask {
    #define VERBOSE_POSTAMBLE \
            VB_LAST_ITEM \
        };
    #define VERBOSE_MAP(name,mask,additive,help) \
        name = (mask),
#else
    // msvc can't have 64bit enums
    #define VERBOSE_PREAMBLE
    #define VERBOSE_POSTAMBLE
    #define VERBOSE_MAP(name,mask,additive,help) \
    const uint64_t name = mask;
#endif

#define LOGLEVEL_PREAMBLE \
    enum LogLevel_t : std::int8_t {
#define LOGLEVEL_POSTAMBLE \
    };
#define LOGLEVEL_MAP(name,value,shortname) \
    name = (value),

#endif

// NOLINTEND(cppcoreguidelines-macro-usage)

VERBOSE_PREAMBLE
VERBOSE_MAP(VB_ALL,       ~0ULL, false,
            "ALL available debug output")
VERBOSE_MAP(VB_MOST,      0xffffffff3ffeffffULL, false,
            "Most debug (nodatabase,notimestamp,noextra)")
#if 0
VERBOSE_MAP(VB_IMPORTANT, 0x00000001, false,
            "Errors or other very important messages")
#endif
VERBOSE_MAP(VB_GENERAL,   0x00000002, true,
            "General info")
VERBOSE_MAP(VB_RECORD,    0x00000004, true,
            "Recording related messages")
VERBOSE_MAP(VB_PLAYBACK,  0x00000008, true,
            "Playback related messages")
VERBOSE_MAP(VB_CHANNEL,   0x00000010, true,
            "Channel related messages")
VERBOSE_MAP(VB_OSD,       0x00000020, true,
            "On-Screen Display related messages")
VERBOSE_MAP(VB_FILE,      0x00000040, true,
            "File and AutoExpire related messages")
VERBOSE_MAP(VB_SCHEDULE,  0x00000080, true,
            "Scheduling related messages")
VERBOSE_MAP(VB_NETWORK,   0x00000100, true,
            "Network protocol related messages")
VERBOSE_MAP(VB_COMMFLAG,  0x00000200, true,
            "Commercial detection related messages")
VERBOSE_MAP(VB_AUDIO,     0x00000400, true,
            "Audio related messages")
VERBOSE_MAP(VB_LIBAV,     0x00000800, true,
            "Enables libav debugging")
VERBOSE_MAP(VB_JOBQUEUE,  0x00001000, true,
            "JobQueue related messages")
VERBOSE_MAP(VB_SIPARSER,  0x00002000, true,
            "Siparser related messages")
VERBOSE_MAP(VB_EIT,       0x00004000, true,
            "EIT related messages")
VERBOSE_MAP(VB_VBI,       0x00008000, true,
            "VBI related messages")
VERBOSE_MAP(VB_DATABASE,  0x00010000, true,
            "Display all SQL commands executed")
VERBOSE_MAP(VB_DSMCC,     0x00020000, true,
            "DSMCC carousel related messages")
VERBOSE_MAP(VB_MHEG,      0x00040000, true,
            "MHEG debugging messages")
VERBOSE_MAP(VB_UPNP,      0x00080000, true,
            "UPnP debugging messages")
VERBOSE_MAP(VB_SOCKET,    0x00100000, true,
            "socket debugging messages")
VERBOSE_MAP(VB_XMLTV,     0x00200000, true,
            "xmltv output and related messages")
VERBOSE_MAP(VB_DVBCAM,    0x00400000, true,
            "DVB CAM debugging messages")
VERBOSE_MAP(VB_MEDIA,     0x00800000, true,
            "Media Manager debugging messages")
VERBOSE_MAP(VB_IDLE,      0x01000000, true,
            "System idle messages")
VERBOSE_MAP(VB_CHANSCAN,  0x02000000, true,
            "Channel Scanning messages")
VERBOSE_MAP(VB_GUI,       0x04000000, true,
            "GUI related messages")
VERBOSE_MAP(VB_SYSTEM,    0x08000000, true,
            "External executable related messages")
#if 0
VERBOSE_MAP(VB_EXTRA,     0x40000000, true,
            "More detailed messages in selected levels")
#endif
VERBOSE_MAP(VB_TIMESTAMP, 0x80000000, true,
            "Conditional data driven messages")
VERBOSE_MAP(VB_PROCESS,   0x100000000ULL, true,
            "MPEG2Fix processing messages")
VERBOSE_MAP(VB_FRAME,     0x200000000ULL, true,
            "MPEG2Fix frame messages")
VERBOSE_MAP(VB_RPLXQUEUE, 0x400000000ULL, true,
            "MPEG2Fix Replex Queue messages")
VERBOSE_MAP(VB_DECODE,    0x800000000ULL, true,
            "MPEG2Fix Decode messages")
VERBOSE_MAP(VB_FLUSH,     0x1000000000ULL, true,
            "")
/* Please do not add a description for VB_FLUSH, it
   should not be passed in via "-v". It is used to
   flush output to the standard output from console
   programs that have debugging enabled.
 */
VERBOSE_MAP(VB_STDIO,     0x2000000000ULL, true,
            "")
/* Please do not add a description for VB_STDIO, it
   should not be passed in via "-v". It is used to
   send output to the standard output from console
   programs that have debugging enabled.
 */
VERBOSE_MAP(VB_GPU,       0x4000000000ULL, true,
            "GPU OpenGL driver messages")
VERBOSE_MAP(VB_GPUAUDIO,  0x8000000000ULL, true,
            "GPU Audio Processing messages")
VERBOSE_MAP(VB_GPUVIDEO,  0x10000000000ULL, true,
            "GPU video rendering messages")
VERBOSE_MAP(VB_REFCOUNT,  0x20000000000ULL, true,
            "Reference Count messages")
VERBOSE_MAP(VB_HTTP,  0x40000000000ULL, true,
            "HTTP Server messages")
VERBOSE_MAP(VB_LIRC,  0x80000000000ULL, true,
            "LIRC Client messages")
VERBOSE_MAP(VB_NONE,      0x00000000, false,
            "NO debug output")
VERBOSE_POSTAMBLE


LOGLEVEL_PREAMBLE
LOGLEVEL_MAP(LOG_ANY,    -1, ' ')
LOGLEVEL_MAP(LOG_EMERG,   0, '!')
LOGLEVEL_MAP(LOG_ALERT,   1, 'A')
LOGLEVEL_MAP(LOG_CRIT,    2, 'C')
LOGLEVEL_MAP(LOG_ERR,     3, 'E')
LOGLEVEL_MAP(LOG_WARNING, 4, 'W')
LOGLEVEL_MAP(LOG_NOTICE,  5, 'N')
LOGLEVEL_MAP(LOG_INFO,    6, 'I')
LOGLEVEL_MAP(LOG_DEBUG,   7, 'D')
LOGLEVEL_MAP(LOG_TRACE,   8, 'T')
LOGLEVEL_MAP(LOG_UNKNOWN, 9, '-')
LOGLEVEL_POSTAMBLE

#ifndef MYTH_IMPLEMENT_VERBOSE
#ifdef __cplusplus
struct VerboseDef {
    uint64_t    mask     {0};
    QString     name;
    bool        additive {false};
    QString     helpText;
};
using VerboseMap = QMap<QString, VerboseDef *>;

struct LoglevelDef {
    int         value     {LOG_UNKNOWN};
    QString     name;
    char        shortname {'-'};
};
using LoglevelMap = QMap<int, LoglevelDef *>;
using ComponentLogLevelMap = QMap<uint64_t, LogLevel_t>;

extern VerboseMap verboseMap;
extern QMutex verboseMapMutex;

extern LoglevelMap loglevelMap;
extern QMutex loglevelMapMutex;
#endif
#endif

#endif
