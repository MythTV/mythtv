#ifndef UPNPEXP_H_
#define UPNPEXP_H_

#include <QtCore/qglobal.h>

#ifdef UPNP_API
# define UPNP_PUBLIC Q_DECL_EXPORT
#else
# define UPNP_PUBLIC Q_DECL_IMPORT
#endif

#endif // UPNPEXP_H_
