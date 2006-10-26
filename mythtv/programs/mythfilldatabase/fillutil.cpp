// Qt headers
#include <qfile.h>
#include <qdir.h>

// libmyth headers
#include "mythcontext.h"

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
            if (m & IO_ReadOnly)
            {
                handle = stdin;
            }
        }
        retval = file.open(m, handle);
    }
    else
    {
        file.setName(filename);
        retval = file.open(m);
    }

    return retval;
}

QString SetupIconCacheDirectory(void)
{
    QString fileprefix = MythContext::GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    fileprefix += "/channels";

    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    return fileprefix;
}
