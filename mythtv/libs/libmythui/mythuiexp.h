#ifndef MYTHUIEXP_H_
#define MYTHUIEXP_H_

#include <QtCore/qglobal.h>

#ifdef MUI_API
# define MUI_PUBLIC Q_DECL_EXPORT
#else
# define MUI_PUBLIC Q_DECL_IMPORT
#endif

#endif // MYTHUIEXP_H_
