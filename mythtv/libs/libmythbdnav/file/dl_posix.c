#if HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_MALLOC_H
#include <malloc.h>
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#if HAVE_DLFCN_H
#include <dlfcn.h>
#else
#include "compat.h"
#endif

#include "dl.h"
#include "../util/macro.h"
#include "../util/logging.h"



// Note the dlopen takes just the name part. "aacs", internally we
// translate to "libaacs.so" "libaacs.dylib" or "aacs.dll".
void   *dl_dlopen  ( const char* name )
{
    char *path;
    int len;
    void *result;

#ifdef __APPLE__
    len = strlen(name) + 3 + 6 + 1;
    path = (char *) malloc(len);
    if (!path) return NULL;
    snprintf(path, len, "lib%s.dylib", name);
#else
    len = strlen(name) + 3 + 3 + 1;
    path = (char *) malloc(len);
    if (!path) return NULL;
    snprintf(path, len, "lib%s.so", name);
#endif
    DEBUG(DBG_BDPLUS, "searching for library '%s' ...\n", path);
    result = dlopen(path, RTLD_LAZY);
    if (!result) {
        DEBUG(DBG_FILE | DBG_CRIT, "can't open library '%s': %s\n", path, dlerror());
    }
    free(path);
    return result;
}

void   *dl_dlsym   ( void* handle, const char* symbol )
{
    void *result = dlsym(handle, symbol);

    if (!result) {
      DEBUG(DBG_FILE | DBG_CRIT, "dlsym(%p, '%s') failed: %s\n", handle, symbol, dlerror());
    }

    return result;
}

int     dl_dlclose ( void* handle )
{
    return dlclose(handle);
}

