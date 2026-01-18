#include "dvdread/mythdvdio.h"

#include "libmythtv/mythtvexp.h"
#include "libmythtv/io/mythiowrapper.h"

MTV_PUBLIC int MythDVD_open(const char *Pathname, int Flags)
{
    return MythFileOpen(Pathname, Flags);
}

MTV_PUBLIC int MythDVD_close(int FileID)
{
    return MythfileClose(FileID);
}

MTV_PUBLIC off_t MythDVD_lseek(int FileID, off_t Offset, int Whence)
{
    return MythFileSeek(FileID, Offset, Whence);
}

MTV_PUBLIC ssize_t MythDVD_read(int FileID, void *Buffer, size_t Count)
{
    return MythFileRead(FileID, Buffer, Count);
}

MTV_PUBLIC int MythDVD_dvdstat(const char *Path, struct stat *Buf)
{
    return MythFileStat(Path, Buf);
}

MTV_PUBLIC int MythDVD_FileExists(const char *Path, const char *File)
{
    return MythFileExists(Path, File);
}
