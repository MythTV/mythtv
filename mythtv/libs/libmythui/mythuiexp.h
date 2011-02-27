#ifndef MYTHUIEXP_H_
#define MYTHUIEXP_H_

#include <QtCore/qglobal.h>

#ifdef MUI_API
# define MUI_PUBLIC Q_DECL_EXPORT
#else
# define MUI_PUBLIC Q_DECL_IMPORT
#endif

#if ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 2)))
# define MHIDDEN        __attribute__((visibility("hidden")))
# define MUNUSED        __attribute__((unused))
# define MDEPRECATED    __attribute__((deprecated))
# define MUNUSED_RESULT __attribute__((warn_unused_result))
#else
# define MHIDDEN
# define MUNUSED
# define MDEPRECATED
# define MUNUSED_RESULT 
#endif

#endif // MYTHUIEXP_H_
