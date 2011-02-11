#ifndef __MYTHIOWRAPPER__
#define __MYTHIOWRAPPER__

#include <sys/stat.h>
#include <sys/types.h>

#include "mythexp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*callback_t)(void*);
void    mythfile_open_register_callback(const char *pathname, void* object,
                                        callback_t func);

int     mythfile_check(int fileID);
int     mythfile_open(const char *pathname, int flags);
int     mythfile_close(int fileID);
#ifdef USING_MINGW
off64_t mythfile_seek(int fileID, off64_t offset, int whence);
off64_t mythfile_tell(int fileID);
#else
off_t   mythfile_seek(int fileID, off_t offset, int whence);
off_t   mythfile_tell(int fileID);
#endif
ssize_t mythfile_read(int fileID, void *buf, size_t count);
ssize_t mythfile_write(int fileID, void *buf, size_t count);
MPUBLIC int     mythfile_stat(const char *path, struct stat *buf);
int     mythfile_stat_fd(int fileID, struct stat *buf);
int     mythfile_exists(const char *path, const char *file);

int     mythdir_check(int fileID);
int     mythdir_opendir(const char *dirname);
int     mythdir_closedir(int dirID);
char   *mythdir_readdir(int dirID);

#ifdef __cplusplus
}
#endif

#endif

