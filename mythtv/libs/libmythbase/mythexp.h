#ifndef MYTHEXP_H_
#define MYTHEXP_H_

#if (__GNUC__ >= 4)
#define MHIDDEN __attribute__((visibility("hidden")))
#define MPUBLIC __attribute__((visibility("default")))
#define MUNUSED __attribute__((unused))
#define MDEPRECATED __attribute__((deprecated))
#else
#define MHIDDEN
#define MUNUSED
#define MDEPRECATED
#ifdef _MSC_VER
  #ifdef MYTH_API
    #define MPUBLIC __declspec( dllexport )
  #else
    #define MPUBLIC __declspec( dllimport )
  #endif

#else
  #define MPUBLIC
#endif
#endif


#endif // MYTHEXP_H_
