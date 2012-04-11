#ifndef PLIST_H
#define PLIST_H

#include <QVariant>
#include <QXmlStreamWriter>
#include "mythbaseexp.h"

class MBASE_PUBLIC PList
{
  public:
    PList(const QByteArray &data);

    QVariant GetValue(const QString &key);
    QString  ToString(void);
    bool     ToXML(QIODevice *device);

  private:
    void            ParseBinaryPList(const QByteArray &data);
    QVariant        ParseBinaryNode(quint64 num);
    QVariantMap     ParseBinaryDict(quint8 *data);
    QList<QVariant> ParseBinaryArray(quint8 *data);
    QVariant        ParseBinaryString(quint8 *data);
    QVariant        ParseBinaryReal(quint8 *data);
    QVariant        ParseBinaryDate(quint8 *data);
    QVariant        ParseBinaryData(quint8 *data);
    QVariant        ParseBinaryUnicode(quint8 *data);
    QVariant        ParseBinaryUInt(quint8 **data);
    quint64         GetBinaryCount(quint8 **data);
    quint64         GetBinaryUInt(quint8 *p, quint64 size);
    quint8*         GetBinaryObject(quint64 num);

  private:
    bool ToXML(const QVariant &data, QXmlStreamWriter &xml);
    void DictToXML(const QVariant &data, QXmlStreamWriter &xml);
    void ArrayToXML(const QVariant &data, QXmlStreamWriter &xml);

    QVariant m_result;
    quint8  *m_data;
    quint8  *m_offsetTable;
    quint64  m_rootObj;
    quint64  m_numObjs;
    quint8   m_offsetSize;
    quint8   m_parmSize;
};

#endif // PLIST_H
