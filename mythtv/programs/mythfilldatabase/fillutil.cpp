// Qt headers
#include <QDir>
#include <QFile>

// MythTV headers
#include "libmythbase/mythdirs.h"

// filldata headers
#include "fillutil.h"

bool dash_open(QFile &file, const QString &filename, QIODevice::OpenMode m, FILE *handle)
{
    bool retval = false;
    if (filename == "-")
    {
        if (handle == nullptr)
        {
            handle = stdout;
            if (m & QIODevice::ReadOnly)
            {
                handle = stdin;
            }
        }
        retval = file.open(handle, m);
    }
    else
    {
        file.setFileName(filename);
        retval = file.open(m);
    }

    return retval;
}

QString SetupIconCacheDirectory(void)
{
    QString fileprefix = GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    fileprefix += "/channels";

    dir.setPath(fileprefix);;
    if (!dir.exists())
        dir.mkdir(fileprefix);

    return fileprefix;
}
