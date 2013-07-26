#ifndef __MYTHIOWRAPPER__
#define __MYTHIOWRAPPER__

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>

#include "mythtvexp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*callback_t)(void*);
void    mythfile_open_register_callback(const char *pathname, void* object,
                                        callback_t func);

int     mythfile_check(int fileID);
MTV_PUBLIC int     mythfile_open(const char *pathname, int flags);
MTV_PUBLIC int     mythfile_close(int fileID);
#ifdef _WIN32
MTV_PUBLIC off64_t mythfile_seek(int fileID, off64_t offset, int whence);
MTV_PUBLIC off64_t mythfile_tell(int fileID);
#else
MTV_PUBLIC off_t   mythfile_seek(int fileID, off_t offset, int whence);
MTV_PUBLIC off_t   mythfile_tell(int fileID);
#endif
MTV_PUBLIC ssize_t mythfile_read(int fileID, void *buf, size_t count);
MTV_PUBLIC ssize_t mythfile_write(int fileID, void *buf, size_t count);
MTV_PUBLIC int     mythfile_stat(const char *path, struct stat *buf);
MTV_PUBLIC int     mythfile_stat_fd(int fileID, struct stat *buf);
int     mythfile_exists(const char *path, const char *file);

int     mythdir_check(int fileID);
MTV_PUBLIC int     mythdir_opendir(const char *dirname);
MTV_PUBLIC int     mythdir_closedir(int dirID);
MTV_PUBLIC char   *mythdir_readdir(int dirID);

#ifdef __cplusplus
}
#endif

#endif

