#ifndef __FILE_MYTHIOWRAPPER__
#define __FILE_MYTHIOWRAPPER__

#include "file.h"

BD_FILE_H *file_open_mythiowrapper(const char* filename, const char *mode);
BD_DIR_H  *dir_open_mythiowrapper(const char* dirname);

#endif

