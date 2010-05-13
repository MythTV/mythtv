// Qt headers
#include <QFile>
#include <QDir>

// libmyth headers
#include "mythdirs.h"

// filldata headers
#include "fillutil.h"

bool dash_open(QFile &file, const QString &filename, int m, FILE *handle)
{
    bool retval = false;
    if (filename == "-")
    {
        if (handle == NULL)
        {
            handle = stdout;
            if (m & QIODevice::ReadOnly)
            {
                handle = stdin;
            }
        }
        retval = file.open( handle, (QIODevice::OpenMode)m );
    }
    else
    {
        file.setFileName(filename);
        retval = file.open( (QIODevice::OpenMode)m );
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

    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    return fileprefix;
}
