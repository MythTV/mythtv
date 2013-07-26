#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <strings.h>

#include "file.h"
#include "util/macro.h"
#include "util/logging.h"

#include "mythiowrapper.h"
#include "file_mythiowrapper.h"  // to pass GCC -Werror=missing-prototypes

static void file_close_mythiowrapper(BD_FILE_H *file)
{
    if (file) {
        mythfile_close((int)file->internal);

        BD_DEBUG(DBG_FILE, "Closed mythfile file (%p)\n", file);

        X_FREE(file);
    }
}

static int64_t file_seek_mythiowrapper(BD_FILE_H *file, int64_t offset, int32_t origin)
{
    return mythfile_seek((int)file->internal, offset, origin);
}

static int64_t file_tell_mythiowrapper(BD_FILE_H *file)
{
    return mythfile_tell((int)file->internal);
}

static int file_eof_mythiowrapper(BD_FILE_H *file)
{
    // FIXME
    // return mythfile_eof((FILE *)file->internal);

    fprintf(stderr, "file_eof_mythiowrapper() ERROR UNIMPLEMENTED\n");
    return 0;
}

static int file_stat_mythiowrapper(BD_FILE_H *file, struct stat *buf)
{
    return mythfile_stat_fd((int)file->internal, buf);
}

static int64_t file_read_mythiowrapper(BD_FILE_H *file, uint8_t *buf, int64_t size)
{
    return mythfile_read((int)file->internal, buf, size);
}

static int64_t file_write_mythiowrapper(BD_FILE_H *file, const uint8_t *buf, int64_t size)
{
    return mythfile_write((int)file->internal, (void *)buf, size);
}

BD_FILE_H *file_open_mythiowrapper(const char* filename, const char *mode)
{
    FILE *fp = NULL;
    BD_FILE_H *file = malloc(sizeof(BD_FILE_H));

    BD_DEBUG(DBG_FILE, "Opening mythfile file %s... (%p)\n", filename, file);
    file->close = file_close_mythiowrapper;
    file->seek = file_seek_mythiowrapper;
    file->read = file_read_mythiowrapper;
    file->write = file_write_mythiowrapper;
    file->tell = file_tell_mythiowrapper;
    file->eof = file_eof_mythiowrapper;
    file->stat = file_stat_mythiowrapper;

    int fd;
    int intMode = O_RDONLY;
    if (!strcasecmp(mode, "wb"))
        intMode = O_WRONLY;

    if ((fd = mythfile_open(filename, intMode)) >= 0) {
        file->internal = (void *)fd;

        return file;
    }

    BD_DEBUG(DBG_FILE, "Error opening file! (%p)\n", file);

    X_FREE(file);

    return NULL;
}

