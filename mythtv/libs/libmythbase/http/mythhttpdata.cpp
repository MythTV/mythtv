// MythTV
#include "mythhttpdata.h"

HTTPData MythHTTPData::Create()
{
    return std::shared_ptr<MythHTTPData>(new MythHTTPData);
}

HTTPData MythHTTPData::Create(const QString &Name, const char *Buffer)
{
    return std::shared_ptr<MythHTTPData>(new MythHTTPData(Name, Buffer));
}

HTTPData MythHTTPData::Create(int Size, char Char)
{
    return std::shared_ptr<MythHTTPData>(new MythHTTPData(Size, Char));
}

HTTPData MythHTTPData::Create(const QByteArray& Other)
{
    return std::shared_ptr<MythHTTPData>(new MythHTTPData(Other));
}

MythHTTPData::MythHTTPData()
  : MythHTTPContent("")
{
}

MythHTTPData::MythHTTPData(const QString& FileName, const char * Buffer)
  : QByteArray(Buffer),
    MythHTTPContent(FileName)
{
    m_cacheType = HTTPETag;
}

MythHTTPData::MythHTTPData(int Size, char Char)
  : QByteArray(Size, Char),
    MythHTTPContent("")
{
}

MythHTTPData::MythHTTPData(const QByteArray& Other)
  : QByteArray(Other),
    MythHTTPContent("")
{
}
