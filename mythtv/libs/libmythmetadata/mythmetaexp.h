#ifndef MYTHMETAEXP_H_
#define MYTHMETAEXP_H_

#include <QtCore/qglobal.h>

#ifdef META_API
# define META_PUBLIC Q_DECL_EXPORT
#else
# define META_PUBLIC Q_DECL_IMPORT
#endif

#endif // MYTHMETAEXP_H_
