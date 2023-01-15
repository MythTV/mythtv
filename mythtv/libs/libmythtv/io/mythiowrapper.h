#ifndef MYTHIOWRAPPER_H
#define MYTHIOWRAPPER_H

#ifdef __cplusplus
#include <cstring>
#else
#include <string.h>
#endif
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "libmythtv/mythtvexp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*callback_t)(void*); //NOLINT(modernize-use-using) included from C code

void               MythFileOpenRegisterCallback(const char *Pathname, void* Object, callback_t Func);
int                MythFileCheck  (int Id);
MTV_PUBLIC int     MythFileOpen   (const char *Pathname, int Flags);
MTV_PUBLIC int     MythfileClose  (int FileID);
#ifdef _WIN32
MTV_PUBLIC off64_t MythFileSeek   (int FileID, off64_t Offset, int Whence);
MTV_PUBLIC off64_t MythFileTell   (int FileID);
#else
MTV_PUBLIC off_t   MythFileSeek   (int FileID, off_t Offset, int Whence);
MTV_PUBLIC off_t   MythFileTell   (int FileID);
#endif
MTV_PUBLIC ssize_t MythFileRead   (int FileID, void *Buffer, size_t Count);
MTV_PUBLIC ssize_t MythFileWrite  (int FileID, void *Buffer, size_t Count);
MTV_PUBLIC int     MythFileStat   (const char *Path, struct stat *Buf);
MTV_PUBLIC int     MythFileStatFD (int FileID, struct stat *Buf);
MTV_PUBLIC int     MythFileExists (const char *Path, const char *File);
int                MythDirCheck   (int DirID);
MTV_PUBLIC int     MythDirOpen    (const char *DirName);
MTV_PUBLIC int     MythDirClose   (int DirID);
MTV_PUBLIC char*   MythDirRead    (int DirID);

#ifdef __cplusplus
}
#endif
#endif

