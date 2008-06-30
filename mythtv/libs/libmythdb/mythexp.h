#ifndef MYTHEXP_H_
#define MYTHEXP_H_

#if (__GNUC__ >= 4)
#define MHIDDEN __attribute__((visibility("hidden")))
#define MPUBLIC __attribute__((visibility("default")))
#else
#define MHIDDEN
#define MPUBLIC
#endif

#endif // MYTHEXP_H_
