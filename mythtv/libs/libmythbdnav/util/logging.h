
#ifndef LOGGING_H_
#define LOGGING_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include "attributes.h"

#define DEBUG(X,Y,...) debug(__FILE__,__LINE__,X,Y,##__VA_ARGS__)

enum debug_mask_enum {
    DBG_RESERVED = 1,
    DBG_CONFIGFILE = 2,
    DBG_FILE = 4,
    DBG_AACS = 8,
    DBG_MKB = 16,
    DBG_MMC = 32,
    DBG_BLURAY = 64,
    DBG_DIR = 128,
    DBG_NAV = 256,
    DBG_BDPLUS = 512,
    DBG_DLX = 1024,
    DBG_CRIT = 2048          // this is libbluray's default debug mask so use this if you want to display critical info
};

typedef enum debug_mask_enum debug_mask_t;
extern debug_mask_t debug_mask;

char *print_hex(uint8_t *str, int count);
void debug(const char *file, int line, uint32_t mask, const char *format, ...) BD_ATTR_FORMAT_PRINTF(4,5);

#ifdef __cplusplus
};
#endif

#endif /* LOGGING_H_ */
