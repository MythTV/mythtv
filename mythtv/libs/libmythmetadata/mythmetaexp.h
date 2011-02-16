#ifndef MYTHMETAEXP_H_
#define MYTHMETAEXP_H_

#include <QtCore/qglobal.h>

#ifdef META_API
# define META_PUBLIC Q_DECL_EXPORT
#else
# define META_PUBLIC Q_DECL_IMPORT
#endif

#if ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 2)))
# define MHIDDEN     __attribute__((visibility("hidden")))
# define MUNUSED     __attribute__((unused))
# define MDEPRECATED __attribute__((deprecated))
#else
# define MHIDDEN
# define MUNUSED
# define MDEPRECATED
#endif

#endif // MYTHMETAEXP_H_
