#include "libmythbase/mythlogging.h"
#include "io/mythiowrapper.h"
#include "Bluray/mythbdiowrapper.h"

// Std
#include <cstdio>
#include <fcntl.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>

// Bluray
#ifdef HAVE_LIBBLURAY
#include <libbluray/filesystem.h>
#else
#include "file/filesystem.h"
#endif

#define LOC QString("BDIOWrapper: ")

static BD_FILE_OPEN sDefaultFileOpen = nullptr;
static BD_DIR_OPEN  sDefaultDirOpen  = nullptr;

static void MythBDDirClose(BD_DIR_H *Dir)
{
    if (Dir)
    {
        MythDirClose(static_cast<int>(reinterpret_cast<intptr_t>(Dir->internal)));
        LOG(VB_FILE, LOG_DEBUG, LOC + "Closed mythdir dir");
        free(Dir);
    }
}

static int MythBDDirRead(BD_DIR_H *Dir, BD_DIRENT *Entry)
{
    char *filename = MythDirRead(static_cast<int>(reinterpret_cast<intptr_t>(Dir->internal)));
    if (filename)
    {
        Entry->d_name[255] = '\0';
        strncpy(Entry->d_name, filename, 255);
        free(filename);
        return 0;
    }

    return 1;
}

static BD_DIR_H *MythBDDirOpen(const char* DirName)
{
    if ((strncmp(DirName, "myth://", 7) != 0) && (sDefaultDirOpen != nullptr))
    {
        // Use the original directory handling for directories that are
        // not in a storage group.
        return sDefaultDirOpen(DirName);
    }

    auto *dir = static_cast<BD_DIR_H*>(calloc(1, sizeof(BD_DIR_H)));

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("Opening mythdir '%1'").arg(DirName));
    dir->close = MythBDDirClose;
    dir->read = MythBDDirRead;

    int dirID = MythDirOpen(DirName);
    if (dirID != 0)
    {
        dir->internal = reinterpret_cast<void*>(static_cast<intptr_t>(dirID));
        return dir;
    }

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("Error opening dir '%1'").arg(DirName));
    free(dir);
    return nullptr;
}


static void MythBDFileClose(BD_FILE_H *File)
{
    if (File)
    {
        MythfileClose(static_cast<int>(reinterpret_cast<intptr_t>(File->internal)));
        LOG(VB_FILE, LOG_DEBUG, LOC + "Closed mythfile file");
        free(File);
    }
}

static int64_t MythBDFileSeek(BD_FILE_H *File, int64_t Offset, int32_t Origin)
{
    return MythFileSeek(static_cast<int>(reinterpret_cast<intptr_t>(File->internal)), Offset, Origin);
}

static int64_t MythBDFileTell(BD_FILE_H *File)
{
    return MythFileTell(static_cast<int>(reinterpret_cast<intptr_t>(File->internal)));
}

static int64_t MythBDFileRead(BD_FILE_H *File, uint8_t *Buffer, int64_t Size)
{
    return MythFileRead(static_cast<int>(reinterpret_cast<intptr_t>(File->internal)),
                        Buffer, static_cast<size_t>(Size));
}

static int64_t MythBDFileWrite(BD_FILE_H *File, const uint8_t *Buffer, int64_t Size)
{
    return MythFileWrite(static_cast<int>(reinterpret_cast<intptr_t>(File->internal)),
                         reinterpret_cast<void*>(const_cast<uint8_t*>(Buffer)),
                         static_cast<size_t>(Size));
}

static BD_FILE_H *MythBDFileOpen(const char* FileName, const char *CMode)
{
    if ((strncmp(FileName, "myth://", 7) != 0) && (sDefaultFileOpen != nullptr))
    {
        // Use the original file handling for files that are
        // not in a storage group.
        return sDefaultFileOpen(FileName, CMode);
    }

    auto *file = static_cast<BD_FILE_H*>(calloc(1, sizeof(BD_FILE_H)));

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("Opening mythfile file '%1'").arg(FileName));
    file->close = MythBDFileClose;
    file->seek  = MythBDFileSeek;
    file->read  = MythBDFileRead;
    file->write = MythBDFileWrite;
    file->tell  = MythBDFileTell;
    file->eof   = nullptr;

    int intMode = O_RDONLY;
    if (strcasecmp(CMode, "wb") == 0)
        intMode = O_WRONLY;

    int fd = MythFileOpen(FileName, intMode);
    if (fd >= 0)
    {
        file->internal = reinterpret_cast<void*>(static_cast<intptr_t>(fd));
        return file;
    }

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("Error opening file '%1'").arg(FileName));
    free(file);
    return nullptr;
}

void MythBDIORedirect(void)
{
    BD_FILE_OPEN origFile = bd_register_file(MythBDFileOpen);
    BD_DIR_OPEN  origDir  = bd_register_dir(MythBDDirOpen);

    if (sDefaultFileOpen == nullptr)
        sDefaultFileOpen = origFile;

    if (sDefaultDirOpen == nullptr)
        sDefaultDirOpen = origDir;
}
