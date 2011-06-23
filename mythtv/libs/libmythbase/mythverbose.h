#ifndef MYTHVERBOSE_H_
#define MYTHVERBOSE_H_

#ifdef __cplusplus
#   include <QString>
#endif
#include <errno.h>

#include "mythbaseexp.h"  //  MBASE_PUBLIC , et c.
#include "mythlogging.h"
#include "verbosedefs.h"

/// This global variable is set at startup with the flags
/// of the verbose messages we want to see.
extern MBASE_PUBLIC uint64_t print_verbose_messages;

// Helper for checking verbose flags outside of VERBOSE macro
#define VERBOSE_LEVEL_NONE        (print_verbose_messages == 0)
#define VERBOSE_LEVEL_CHECK(mask) ((print_verbose_messages & (mask)) == (mask))

// There are two VERBOSE macros now.  One for use with Qt/C++, one for use
// without Qt.
//
// Neither of them will lock the calling thread, but rather put the log message
// onto a queue.

#ifdef __cplusplus
#define VERBOSE(mask, ...) \
    LogPrintQString((uint64_t)(mask), LOG_INFO, QString(__VA_ARGS__))
#else
#define VERBOSE(mask, ...) \
    LogPrint((uint64_t)(mask), LOG_INFO, __VA_ARGS__)
#endif


#ifdef  __cplusplus
    /// Verbose helper function for ENO macro
    extern MBASE_PUBLIC QString safe_eno_to_string(int errnum);

    extern MBASE_PUBLIC QString verboseString;

    MBASE_PUBLIC int parse_verbose_arg(QString arg);

    /// This can be appended to the VERBOSE args with 
    /// "+".  Please do not use "<<".  It uses
    /// a thread safe version of strerror to produce the
    /// string representation of errno and puts it on the
    /// next line in the verbose output.
    #define ENO QString("\n\t\t\teno: ") + safe_eno_to_string(errno)
#endif


#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
