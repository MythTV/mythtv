#ifndef MYTHMIMEDATABASE_H
#define MYTHMIMEDATABASE_H

// MythTV
#include "http/mythmimetype.h"

class QIODevice;
class MythMimeDatabasePriv;

class MythMimeDatabase
{
  public:
    MythMimeDatabase();

    MythMimeTypes AllTypes() const;
    MythMimeType  MimeTypeForName(const QString& Name) const;
    QString       SuffixForFileName(const QString& FileName) const;
    MythMimeTypes MimeTypesForFileName(const QString& FileName) const;
    MythMimeType  MimeTypeForFileNameAndData(const QString& FileName, const QByteArray& Data);
    MythMimeType  MimeTypeForFileNameAndData(const QString& FileName, QIODevice* Device);

  private:
    MythMimeDatabasePriv* m_priv;
};

#endif
