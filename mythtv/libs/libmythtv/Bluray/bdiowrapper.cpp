#include "config.h"

#include <cstdio>
#include <fcntl.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>

#if CONFIG_LIBBLURAY_EXTERNAL
#include "libbluray/filesystem.h"
#else
#include "file/filesystem.h"
#endif

#include "mythiowrapper.h"
#include "bdiowrapper.h"

#include "mythlogging.h"

#define LOC      QString("BDIOWrapper: ")

static BD_FILE_OPEN sDefaultFileOpen = nullptr;
static BD_DIR_OPEN  sDefaultDirOpen  = nullptr;

static void dir_close_mythiowrapper(BD_DIR_H *dir)
{
    if (dir)
    {
        mythdir_closedir((int)(intptr_t)dir->internal);

        LOG(VB_FILE, LOG_DEBUG, LOC + "Closed mythdir dir");

        free(dir);
    }
}

static int dir_read_mythiowrapper(BD_DIR_H *dir, BD_DIRENT *entry)
{
    char *filename = mythdir_readdir((int)(intptr_t)dir->internal);
    if (filename)
    {
        entry->d_name[255] = '\0';
        strncpy(entry->d_name, filename, 255);
        free(filename);
        return 0;
    }

    return 1;
}

static BD_DIR_H *dir_open_mythiowrapper(const char* dirname)
{
    if ((strncmp(dirname, "myth://", 7) != 0) && (sDefaultDirOpen != nullptr))
    {
        // Use the original directory handling for directories that are
        // not in a storage group.
        return sDefaultDirOpen(dirname);
    }

    auto *dir = (BD_DIR_H*)calloc(1, sizeof(BD_DIR_H));

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("Opening mythdir dir %1...").arg(dirname));
    dir->close = dir_close_mythiowrapper;
    dir->read = dir_read_mythiowrapper;

    int dirID = 0;
    if ((dirID = mythdir_opendir(dirname)))
    {
        dir->internal = (void *)(intptr_t)dirID;
        return dir;
    }

    LOG(VB_FILE, LOG_DEBUG, LOC + "Error opening dir!");

    free(dir);

    return nullptr;
}


static void file_close_mythiowrapper(BD_FILE_H *file)
{
    if (file)
    {
        mythfile_close((int)(intptr_t)file->internal);

        LOG(VB_FILE, LOG_DEBUG, LOC + "Closed mythfile file");

        free(file);
    }
}

static int64_t file_seek_mythiowrapper(BD_FILE_H *file, int64_t offset, int32_t origin)
{
    return mythfile_seek((int)(intptr_t)file->internal, offset, origin);
}

static int64_t file_tell_mythiowrapper(BD_FILE_H *file)
{
    return mythfile_tell((int)(intptr_t)file->internal);
}

static int64_t file_read_mythiowrapper(BD_FILE_H *file, uint8_t *buf, int64_t size)
{
    return mythfile_read((int)(intptr_t)file->internal, buf, size);
}

static int64_t file_write_mythiowrapper(BD_FILE_H *file, const uint8_t *buf, int64_t size)
{
    return mythfile_write((int)(intptr_t)file->internal, (void *)buf, size);
}

static BD_FILE_H *file_open_mythiowrapper(const char* filename, const char *cmode)
{
    if ((strncmp(filename, "myth://", 7) != 0) && (sDefaultFileOpen != nullptr))
    {
        // Use the original file handling for files that are
        // not in a storage group.
        return sDefaultFileOpen(filename, cmode);
    }

    auto *file = (BD_FILE_H*)calloc(1, sizeof(BD_FILE_H));

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("Opening mythfile file %1...").arg(filename));
    file->close = file_close_mythiowrapper;
    file->seek = file_seek_mythiowrapper;
    file->read = file_read_mythiowrapper;
    file->write = file_write_mythiowrapper;
    file->tell = file_tell_mythiowrapper;
    file->eof = nullptr;

    int fd;
    int intMode = O_RDONLY;
    if (strcasecmp(cmode, "wb") == 0)
        intMode = O_WRONLY;

    if ((fd = mythfile_open(filename, intMode)) >= 0)
    {
        file->internal = (void*)(intptr_t)fd;

        return file;
    }

    LOG(VB_FILE, LOG_DEBUG, LOC + "Error opening file!");

    free(file);

    return nullptr;
}

void redirectBDIO()
{
    BD_FILE_OPEN origFile = bd_register_file(file_open_mythiowrapper);
    BD_DIR_OPEN  origDir  = bd_register_dir(dir_open_mythiowrapper);

    if (sDefaultFileOpen == nullptr)
        sDefaultFileOpen = origFile;

    if (sDefaultDirOpen == nullptr)
        sDefaultDirOpen = origDir;
}
