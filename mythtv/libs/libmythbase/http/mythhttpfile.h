#ifndef MYTHHTTPFILE_H
#define MYTHHTTPFILE_H

// Qt
#include <QFile>

// MythTV
#include "http/mythhttptypes.h"
#include "http/mythhttpresponse.h"

class MythHTTPFile : public QFile, public MythHTTPContent
{
  public:
    static HTTPFile     Create      (const QString& ShortName, const QString& FullName);
    static HTTPResponse ProcessFile (HTTPRequest2 Request);

  protected:
    MythHTTPFile(const QString& ShortName, const QString& FullName);

  private:
    Q_DISABLE_COPY(MythHTTPFile)
};

#endif
