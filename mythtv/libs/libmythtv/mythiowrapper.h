#ifndef __MYTHIOWRAPPER__
#define __MYTHIOWRAPPER__

#include <sys/stat.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int     mythfile_check(int fd);
int     mythfile_open(const char *pathname, int flags);
int     mythfile_close(int fd);
off_t   mythfile_seek(int fd, off_t offset, int whence);
ssize_t mythfile_read(int fd, void *buf, size_t count);
ssize_t mythfile_write(int fd, void *buf, size_t count);
int     mythfile_stat(const char *path, struct stat *buf);
int     mythfile_exists(const char *path, const char *file);

int     mythdir_check(int fd);
int     mythdir_opendir(const char *dirname);
int     mythdir_closedir(int dirID);
char   *mythdir_readdir(int dirID);

#ifdef __cplusplus
}
#endif

#endif

