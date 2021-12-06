#ifndef MYTHSERIALISER_H
#define MYTHSERIALISER_H

// Qt
#include <QBuffer>
#include <QMimeType>

// MythTV
#include "http/mythmimetype.h"
#include "http/mythhttpdata.h"

using HTTPMimes = std::vector<MythMimeType>;

class MythSerialiser
{
  public:
    static HTTPData Serialise(const QString& Name, const QVariant& Value, const QStringList& Accept);
    MythSerialiser();
    HTTPData Result();

  protected:
    QBuffer  m_buffer;
    HTTPData m_result { nullptr };
};

#endif
