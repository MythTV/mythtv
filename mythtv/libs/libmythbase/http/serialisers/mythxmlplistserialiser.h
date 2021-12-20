#ifndef MYTHXMLPLISTSERIALISER_H
#define MYTHXMLPLISTSERIALISER_H

// Qt
#include <QXmlStreamWriter>

// MythTV
#include "http/serialisers/mythserialiser.h"

#define XML_PLIST_SERIALIZER_VERSION "1.1"

class MythXMLPListSerialiser : public MythSerialiser
{
  public:
    MythXMLPListSerialiser(const QString& Name, const QVariant& Value);

  protected:
    void AddObject    (const QString& Name, const QVariant& Value);
    void AddValue     (const QString& Name, const QVariant& Value, bool NeedKey = true);
    void AddQObject   (const QString& Name, const QObject* Object);
    void AddStringList(const QString& Name, const QVariant& Values);
    void AddList      (const QString& Name, const QVariantList& Values);
    void AddMap       (const QString& Name, const QVariantMap& Map);
    void AddProperty  (const QString& Name, const QVariant& Value,
                       const QMetaObject* MetaObject, const QMetaProperty* MetaProperty);

  private:
    Q_DISABLE_COPY(MythXMLPListSerialiser)
    static QString GetItemName(const QString& Name);
    static QString GetContentName(const QString &Name, const QMetaObject* MetaObject);
    QXmlStreamWriter m_writer;
};
#endif
