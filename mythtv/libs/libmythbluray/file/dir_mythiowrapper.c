#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "file.h"
#include "util/macro.h"
#include "util/logging.h"

#include "mythiowrapper.h"
#include "file_mythiowrapper.h"  // to pass GCC -Werror=missing-prototypes

static void dir_close_mythiowrapper(BD_DIR_H *dir)
{
    if (dir) {
        mythdir_closedir((int)dir->internal);

        BD_DEBUG(DBG_DIR, "Closed mythdir dir (%p)\n", dir);

        X_FREE(dir);
    }
}

static int dir_read_mythiowrapper(BD_DIR_H *dir, BD_DIRENT *entry)
{
    char *filename = mythdir_readdir((int)dir->internal);
    if (filename)
    {
        strncpy(entry->d_name, filename, 256);
        free(filename);
        return 0;
    }

    return 1;
}

BD_DIR_H *dir_open_mythiowrapper(const char* dirname)
{
    BD_DIR_H *dir = malloc(sizeof(BD_DIR_H));

    BD_DEBUG(DBG_DIR, "Opening mythdir dir %s... (%p)\n", dirname, dir);
    dir->close = dir_close_mythiowrapper;
    dir->read = dir_read_mythiowrapper;

    int dirID = 0;
    if ((dirID = mythdir_opendir(dirname))) {
        dir->internal = (void *)dirID;

        return dir;
    }

    BD_DEBUG(DBG_DIR, "Error opening dir! (%p)\n", dir);

    X_FREE(dir);

    return NULL;
}

