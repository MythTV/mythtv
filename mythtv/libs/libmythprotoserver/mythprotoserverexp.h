#ifndef MYTHPROTOEXP_H_
#define MYTHPROTOEXP_H_

#include <QtCore/qglobal.h>

#ifdef PROTOSERVER_API
# define PROTOSERVER_PUBLIC Q_DECL_EXPORT
#else
# define PROTOSERVER_PUBLIC Q_DECL_IMPORT
#endif

#endif // MYTHPROTOEXP_H_
