#ifndef MYTHTVEXP_H_
#define MYTHTVEXP_H_

#include <QtGlobal>
#ifdef MTV_API
# define MTV_PUBLIC Q_DECL_EXPORT
#else
# define MTV_PUBLIC Q_DECL_IMPORT
#endif

#endif // MYTHTVEXP_H_
