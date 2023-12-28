#ifndef MYTHJSONSERIALISER_H
#define MYTHJSONSERIALISER_H

// MythTV
#include <http/serialisers/mythserialiser.h>

// Std
#include <stack>

class MythJSONSerialiser : public MythSerialiser
{
  public:
    MythJSONSerialiser(const QString& Name, const QVariant& Value);

  protected:
    void AddObject    (const QString&     Name, const QVariant& Value);
    void AddValue     (const QVariant&    Value, const QMetaProperty *MetaProperty = nullptr);
    void AddQObject   (const QObject*     Object);
    void AddStringList(const QVariant&    Values);
    void AddList      (const QVariant&    Values);
    void AddMap       (const QVariantMap& Map);
    static QString Encode(const QString&  Value);

  private:
    Q_DISABLE_COPY(MythJSONSerialiser)
    QTextStream      m_writer;
    std::stack<bool> m_first;
};

#endif
