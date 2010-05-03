#include <stdlib.h>

#include "logging.h"

char out[512];
debug_mask_t debug_mask = 0;
static int debug_init = 0;
FILE *logfile = NULL;


char *print_hex(uint8_t *buf, int count)
{
    memset(out, 0, sizeof(out));

    int zz;
    for(zz = 0; zz < count; zz++) {
        sprintf(out + (zz * 2), "%02X", buf[zz]);
    }

    return out;
}

void debug(const char *file, int line, uint32_t mask, const char *format, ...)
{
    // Only call getenv() once.
    if (!debug_init) {
        char *env;

        debug_init = 1;

        if ((env = getenv("BD_DEBUG_MASK"))) {
            debug_mask = strtol(env, NULL, 0);
        } else {
            debug_mask = DBG_CRIT;
        }

        // Send DEBUG to file?
        if ((env = getenv("BD_DEBUG_FILE"))) {
            logfile = fopen(env, "wb");
            setvbuf(logfile, NULL, _IOLBF, 0);
        }

        if (!logfile)
            logfile = stderr;
    }

    if (mask & debug_mask) {
        char buffer[512];
        va_list args;

        va_start(args, format);
        vsprintf(buffer, format, args);
        va_end(args);

        fprintf(logfile, "%s:%d: %s", file, line, buffer);
    }
}
