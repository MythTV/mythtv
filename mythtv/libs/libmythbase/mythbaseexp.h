#ifndef MYTHBASEEXP_H_
#define MYTHBASEEXP_H_

// This header is called from some non-QT projects,
// and if non C++ then Q_DECL_XXX never defined

#if defined( QT_CORE_LIB ) && defined( __cplusplus )
# include <QtCore/qglobal.h>
# ifdef MBASE_API
#  define MBASE_PUBLIC Q_DECL_EXPORT
# else
#  define MBASE_PUBLIC Q_DECL_IMPORT
# endif
#else
# define MBASE_PUBLIC
#endif

#define MOVERRIDE
#define MFINAL
#if (defined (__cplusplus) && (__cplusplus >= 201103L))
#  if ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 7))) || \
      (defined(__ICC) || defined(__INTEL_COMPILER))
#    undef MOVERRIDE
#    undef MFINAL
#    define MOVERRIDE   override
#    define MFINAL      final
#  endif
#endif

#endif // MYTHBASEEXP_H_
