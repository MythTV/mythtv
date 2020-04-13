
#ifndef EXIV2API_H
#define EXIV2API_H

#ifdef exiv2lib_STATIC
#  define EXIV2API
#  define EXIV2LIB_NO_EXPORT
#else
#  ifndef EXIV2API
#    ifdef exiv2lib_EXPORTS
        /* We are building this library */
#      define EXIV2API __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define EXIV2API __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef EXIV2LIB_NO_EXPORT
#    define EXIV2LIB_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef EXIV2LIB_DEPRECATED
#  define EXIV2LIB_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef EXIV2LIB_DEPRECATED_EXPORT
#  define EXIV2LIB_DEPRECATED_EXPORT EXIV2API EXIV2LIB_DEPRECATED
#endif

#ifndef EXIV2LIB_DEPRECATED_NO_EXPORT
#  define EXIV2LIB_DEPRECATED_NO_EXPORT EXIV2LIB_NO_EXPORT EXIV2LIB_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef EXIV2LIB_NO_DEPRECATED
#    define EXIV2LIB_NO_DEPRECATED
#  endif
#endif

#endif /* EXIV2API_H */
