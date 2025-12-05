#ifndef MYTHEXP_H_
#define MYTHEXP_H_

#include <QtGlobal>

#ifdef MYTH_API
# define MPUBLIC Q_DECL_EXPORT
#else
# define MPUBLIC Q_DECL_IMPORT
#endif

#endif /* MYTHEXP_H_ */
