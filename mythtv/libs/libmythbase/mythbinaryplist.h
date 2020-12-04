#ifndef PLIST_H
#define PLIST_H

// Qt
#include <QVariant>
#include <QXmlStreamWriter>

// MythTV
#include "mythbaseexp.h"

class MBASE_PUBLIC MythBinaryPList
{
  public:
    explicit MythBinaryPList(const QByteArray& Data);
    QVariant GetValue (const QString& Key);
    QString  ToString ();
    bool     ToXML    (QIODevice* Device);

  private:
    void            ParseBinaryPList  (const QByteArray& Data);
    QVariant        ParseBinaryNode   (uint64_t Num);
    QVariantMap     ParseBinaryDict   (uint8_t* Data);
    QList<QVariant> ParseBinaryArray  (uint8_t* Data);
    static QVariant ParseBinaryString (uint8_t* Data);
    static QVariant ParseBinaryReal   (uint8_t* Data);
    static QVariant ParseBinaryDate   (uint8_t* Data);
    static QVariant ParseBinaryData   (uint8_t* Data);
    static QVariant ParseBinaryUnicode(uint8_t* Data);
    static QVariant ParseBinaryUInt   (uint8_t** Data);
    static uint64_t GetBinaryCount    (uint8_t** Data);
    static uint64_t GetBinaryUInt     (uint8_t* Data, uint64_t Size);
    uint8_t*        GetBinaryObject   (uint64_t Num);
    bool            ToXML             (const QVariant& Data, QXmlStreamWriter& Xml);
    void            DictToXML         (const QVariant& Data, QXmlStreamWriter& Xml);
    void            ArrayToXML        (const QVariant& Data, QXmlStreamWriter& Xml);

    QVariant m_result;
    uint8_t* m_data        { nullptr };
    uint8_t* m_offsetTable { nullptr };
    uint64_t m_rootObj     { 0 };
    uint64_t m_numObjs     { 0 };
    uint8_t  m_offsetSize  { 0 };
    uint8_t  m_parmSize    { 0 };
};

#endif
