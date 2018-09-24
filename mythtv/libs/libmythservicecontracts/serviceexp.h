#ifndef SERVICEEXP_H_
#define SERVICEEXP_H_

#include <QtCore/qglobal.h>

#ifdef SERVICE_API
# define SERVICE_PUBLIC Q_DECL_EXPORT
#else
# define SERVICE_PUBLIC Q_DECL_IMPORT
#endif

#endif // SERVICEEXP_H_
