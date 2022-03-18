#ifndef MYTHBASEEXP_H_
#define MYTHBASEEXP_H_

// This header is called from some non-QT projects,
// and if non C++ then Q_DECL_XXX never defined

#if defined( QT_CORE_LIB ) && defined( __cplusplus )
# include <QtCore/qglobal.h>
# if defined(MBASE_API) || defined(MPLUGIN_API)
#  define MBASE_PUBLIC Q_DECL_EXPORT
# else
#  define MBASE_PUBLIC Q_DECL_IMPORT
# endif
#else
# define MBASE_PUBLIC
#endif

#endif // MYTHBASEEXP_H_
