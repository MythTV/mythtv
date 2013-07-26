#ifndef NZMQTEXP_H_
#define NZMQTEXP_H_

#include <QtCore/qglobal.h>

#ifdef NZMQT_API
# define NZMQT_PUBLIC Q_DECL_EXPORT
#else
# define NZMQT_PUBLIC Q_DECL_IMPORT
#endif

#endif // NZMQTEXP_H_
