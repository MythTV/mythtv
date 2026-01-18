#ifndef MYTHIOWRAPPER_H
#define MYTHIOWRAPPER_H

#include <sys/stat.h>
#include <sys/types.h>

#include "libmythtv/mythtvexp.h"

using callback_t = void (*)(void*);
extern "C" {
void               MythFileOpenRegisterCallback(const char *Pathname, void* Object, callback_t Func);
int                MythFileCheck  (int Id);
MTV_PUBLIC int     MythFileOpen   (const char *Pathname, int Flags);
MTV_PUBLIC int     MythfileClose  (int FileID);
MTV_PUBLIC off_t   MythFileSeek   (int FileID, off_t Offset, int Whence);
MTV_PUBLIC off_t   MythFileTell   (int FileID);
MTV_PUBLIC ssize_t MythFileRead   (int FileID, void *Buffer, size_t Count);
MTV_PUBLIC ssize_t MythFileWrite  (int FileID, void *Buffer, size_t Count);
MTV_PUBLIC int     MythFileStat   (const char *Path, struct stat *Buf);
MTV_PUBLIC int     MythFileStatFD (int FileID, struct stat *Buf);
MTV_PUBLIC int     MythFileExists (const char *Path, const char *File);
int                MythDirCheck   (int DirID);
MTV_PUBLIC int     MythDirOpen    (const char *DirName);
MTV_PUBLIC int     MythDirClose   (int DirID);
MTV_PUBLIC char*   MythDirRead    (int DirID);
}
#endif

