#ifndef MYTHCBORSERIALISER_H
#define MYTHCBORSERIALISER_H

// MythTV
#include "http/serialisers/mythserialiser.h"

class QCborStreamWriter;

class MythCBORSerialiser : public MythSerialiser
{
  public:
    MythCBORSerialiser(const QString& Name, const QVariant& Value);

  protected:
    void AddObject    (const QString&     Name, const QVariant& Value);
    void AddValue     (const QVariant&    Value);
    void AddQObject   (const QObject*     Object);
    void AddStringList(const QVariant&    Values);
    void AddList      (const QVariant&    Values);
    void AddMap       (const QVariantMap& Map);

  private:
    Q_DISABLE_COPY(MythCBORSerialiser)
    QCborStreamWriter* m_writer { nullptr };
};

#endif
