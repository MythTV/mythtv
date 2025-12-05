#ifndef MYTHTVEXP_H_
#define MYTHTVEXP_H_

// Included by libdvdnav and libdvdread C code

#ifdef __cplusplus
# include <QtGlobal>
# ifdef MTV_API
#  define MTV_PUBLIC Q_DECL_EXPORT
# else
#  define MTV_PUBLIC Q_DECL_IMPORT
# endif
#else
# define MTV_PUBLIC
#endif

#endif // MYTHTVEXP_H_
