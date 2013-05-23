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

#if ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 2)))
# define MHIDDEN     __attribute__((visibility("hidden")))
# define MUNUSED     __attribute__((unused))
# define MDEPRECATED __attribute__((deprecated))
#else
# define MHIDDEN
# define MUNUSED
# define MDEPRECATED
#endif

#if ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 3)))
# define MERROR(x)   __attribute__((error(x)))
#else
# define MERROR(x)
#endif

#define MOVERRIDE
#define MFINAL
#if (__cplusplus >= 201103L)
#  if ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 7))) || \
      (defined(__ICC) || defined(__INTEL_COMPILER))
#    undef MOVERRIDE
#    undef MFINAL
#    define MOVERRIDE   override
#    define MFINAL      final
#  endif
#endif

#endif // MYTHBASEEXP_H_
