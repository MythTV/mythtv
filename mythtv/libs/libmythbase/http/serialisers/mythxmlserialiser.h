#ifndef MYTHXMLSERIALISER_H
#define MYTHXMLSERIALISER_H

// Qt
#include <QXmlStreamWriter>

// MythTV
#include "http/serialisers/mythserialiser.h"

#define XML_SERIALIZER_VERSION "1.1"

class MythXMLSerialiser : public MythSerialiser
{
  public:
    MythXMLSerialiser(const QString& Name, const QVariant& Value);

  protected:
    void AddObject    (const QString& Name, const QVariant& Value);
    void AddValue     (const QString& Name, const QVariant& Value);
    void AddQObject   (const QObject* Object);
    void AddStringList(const QVariant& Values);
    void AddList      (const QString& Name, const QVariant& Values);
    void AddMap       (const QString& Name, const QVariantMap& Map);
    void AddProperty  (const QString& Name, const QVariant& Value,
                       const QMetaObject* MetaObject, const QMetaProperty* MetaProperty);

  private:
    Q_DISABLE_COPY(MythXMLSerialiser)
    static QString GetItemName(const QString& Name);
    static QString GetContentName(const QString &Name, const QMetaObject* MetaObject);
    QXmlStreamWriter m_writer;
    bool             m_first  { true };
};

#endif
