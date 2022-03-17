#ifndef MYTHHTTPDATA_H
#define MYTHHTTPDATA_H

// Qt
#include <QByteArray>

// MythTV
#include "libmythbase/http/mythhttptypes.h"

class MBASE_PUBLIC MythHTTPData : public QByteArray, public MythHTTPContent
{
  public:
    static HTTPData Create();
    static HTTPData Create(const QString& Name, const char * Buffer);
    static HTTPData Create(int Size, char Char);
    static HTTPData Create(const QByteArray& Other);

  protected:
    MythHTTPData();
    MythHTTPData(const QString& FileName, const char * Buffer);
    MythHTTPData(int Size, char Char);
    explicit MythHTTPData(const QByteArray& Other);

  private:
    Q_DISABLE_COPY(MythHTTPData)
};

#endif
