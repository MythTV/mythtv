#ifndef MYTHHTTPFILE_H
#define MYTHHTTPFILE_H

// Qt
#include <QFile>

// MythTV
#include "libmythbase/http/mythhttptypes.h"

class MythHTTPFile : public QFile, public MythHTTPContent
{
  public:
    static HTTPFile     Create      (const QString& ShortName, const QString& FullName);
    static HTTPResponse ProcessFile (const HTTPRequest2& Request);

  protected:
    MythHTTPFile(const QString& ShortName, const QString& FullName);

  private:
    Q_DISABLE_COPY(MythHTTPFile)
};

#endif
