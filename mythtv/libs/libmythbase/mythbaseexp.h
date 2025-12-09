#ifndef MYTHBASEEXP_H_
#define MYTHBASEEXP_H_

#include <QtGlobal>
#ifdef MBASE_API
# define MBASE_PUBLIC Q_DECL_EXPORT
#else
# define MBASE_PUBLIC Q_DECL_IMPORT
#endif

#endif // MYTHBASEEXP_H_
