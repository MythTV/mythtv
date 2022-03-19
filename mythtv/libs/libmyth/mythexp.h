#ifndef MYTHEXP_H_
#define MYTHEXP_H_

#ifdef __cplusplus
#include <QtCore/qglobal.h>

#if defined(MYTH_API)
# define MPUBLIC Q_DECL_EXPORT
#else
# define MPUBLIC Q_DECL_IMPORT
#endif
#endif /* __cplusplus */

#endif /* MYTHEXP_H_ */
