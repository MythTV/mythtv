#ifndef MYTHMIMEDATABASE_H
#define MYTHMIMEDATABASE_H

// MythTV
#include "libmythbase/http/mythmimetype.h"

class QIODevice;
class MythMimeDatabasePriv;

class MythMimeDatabase
{
  public:
    MythMimeDatabase();

    static MythMimeTypes AllTypes();
    static MythMimeType  MimeTypeForName(const QString& Name);
    static QString       SuffixForFileName(const QString& FileName);
    static MythMimeTypes MimeTypesForFileName(const QString& FileName);
    static MythMimeType  MimeTypeForFileNameAndData(const QString& FileName, const QByteArray& Data);
    static MythMimeType  MimeTypeForFileNameAndData(const QString& FileName, QIODevice* Device);

  private:
    MythMimeDatabasePriv* m_priv {nullptr};
};

#endif
