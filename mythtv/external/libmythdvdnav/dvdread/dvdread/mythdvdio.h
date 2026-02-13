#ifndef MYTHDVDIO_H
#define MYTHDVDIO_H

#include <sys/stat.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int MythDVD_open(const char *Pathname, int Flags);
extern int MythDVD_close(int FileID);
extern off_t MythDVD_lseek(int FileID, off_t Offset, int Whence);
extern ssize_t MythDVD_read(int FileID, void *Buffer, size_t Count);
extern int MythDVD_dvdstat(const char *Path, struct stat *Buf);
extern int MythDVD_FileExists(const char *Path, const char *File);

#ifdef __cplusplus
}
#endif
#endif // MYTHDVDIO_H
