#ifndef MYTHTVEXP_H_
#define MYTHTVEXP_H_

// This header is called from some non-QT projects, 
// and if non C++ then Q_DECL_XXX never defined

#if defined( QT_CORE_LIB ) && defined( __cplusplus )
# include <QtCore/qglobal.h>
# ifdef MTV_API
#  define MTV_PUBLIC Q_DECL_EXPORT
# else
#  define MTV_PUBLIC Q_DECL_IMPORT
# endif
#else
# define MTV_PUBLIC
#endif

#if ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 2)))
# define MHIDDEN     __attribute__((visibility("hidden")))
# define MUNUSED     __attribute__((unused))
# define MDEPRECATED __attribute__((deprecated))
#else
# define MHIDDEN
# define MUNUSED
# define MDEPRECATED
#endif

#endif // MYTHTVEXP_H_
