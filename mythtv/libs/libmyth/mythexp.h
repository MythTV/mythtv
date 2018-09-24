#ifndef MYTHEXP_H_
#define MYTHEXP_H_

#ifdef __cplusplus
#include <QtCore/qglobal.h>

#if defined(MYTH_API) || defined(MPLUGIN_API)
# define MPUBLIC Q_DECL_EXPORT
#else
# define MPUBLIC Q_DECL_IMPORT
#endif
#endif /* __cplusplus */

#if ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 2)))
# define MDEPRECATED __attribute__((deprecated))
#else
# define MDEPRECATED
#endif

#endif /* MYTHEXP_H_ */
