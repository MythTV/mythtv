#if linux
#include <sys/io.h>
#elif defined(__FreeBSD__)
#include <machine/sysarch.h>
#include <machine/cpufunc.h>
#define ioperm i386_set_ioperm
#endif
