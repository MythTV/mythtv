#include "mythdvdvfs.h"

#include <fcntl.h>

#include <cstring>

#include "dvdread/dvd_filesystem.h"

#include "libmythtv/mythtvexp.h"
#include "libmythtv/io/mythiowrapper.h"

extern "C" {
MTV_PUBLIC inline void    MythDVD_close     ([[maybe_unused]]dvd_reader_filesystem_h * fs)
{
}

MTV_PUBLIC inline int     MythDVD_stat      ([[maybe_unused]]dvd_reader_filesystem_h *fs,
    const char *path,
    dvdstat_t *statbuf)
{
    struct stat fileinfo {};
    int ret = MythFileStat(path, &fileinfo);
    if (ret == 0)
    {
        statbuf->size    = fileinfo.st_size;
        statbuf->st_mode = fileinfo.st_mode;
    }
    return ret;
}

MTV_PUBLIC inline void   *MythDVD_dir_open  ([[maybe_unused]]dvd_reader_filesystem_h *fs,
    const char *dirname)
{
    int* ret = new int;
    *ret = MythDirOpen(dirname);
    return static_cast<void*>(ret);
}

MTV_PUBLIC inline int     MythDVD_dir_read  (void *dir, dvd_dirent_t *entry)
{
    std::string s = MythDirRead(*static_cast<int*>(dir));
    if (s.empty())
    {
        return 1; // end of directory or error
    }
    auto size = s.size();
    if (size > DVDREAD_NAME_MAX)
    {
        return -1; // This would truncate.
    }
    memcpy(entry->d_name, s.c_str(), size + 1);
    return 0;
}

MTV_PUBLIC inline void    MythDVD_dir_close (void *dir)
{
    MythDirClose(*static_cast<int*>(dir));
    delete static_cast<int*>(dir);
}

MTV_PUBLIC inline void   *MythDVD_file_open ([[maybe_unused]]dvd_reader_filesystem_h *fs,
    const char *filename)
{
    int* ret = new int;
    *ret = MythFileOpen(filename, O_RDONLY);
    return static_cast<void*>(ret);
}

MTV_PUBLIC inline ssize_t MythDVD_file_read (void *file, char *buf, size_t size)
{
    return MythFileRead(*static_cast<int*>(file), buf, size);
}

MTV_PUBLIC inline dvd_off_t MythDVD_file_seek (void *file, dvd_off_t offset, int whence)
{
    return MythFileSeek(*static_cast<int*>(file), offset, whence);
}

MTV_PUBLIC inline int     MythDVD_file_close(void *file)
{
    int ret = MythfileClose(*static_cast<int*>(file));
    delete static_cast<int*>(file);
    return ret;
}
} // extern "C"

dvd_reader_filesystem_h s_vfs =
{
    .internal   = nullptr,
    .close      = &MythDVD_close,
    .stat       = &MythDVD_stat,
    .dir_open   = &MythDVD_dir_open,
    .dir_read   = &MythDVD_dir_read,
    .dir_close  = &MythDVD_dir_close,
    .file_open  = &MythDVD_file_open,
    .file_read  = &MythDVD_file_read,
    .file_seek  = &MythDVD_file_seek,
    .file_close = &MythDVD_file_close
};
