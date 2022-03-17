#ifndef MYTHHTTPMETASERVICE_H
#define MYTHHTTPMETASERVICE_H

// Qt
#include <QMetaObject>

// MythTV
#include "libmythbase/http/mythhttpmetamethod.h"

class MBASE_PUBLIC MythHTTPMetaService
{
  public:
    MythHTTPMetaService(const QString& Name, const QMetaObject& Meta,
                        const HTTPRegisterTypes& RegisterCallback = nullptr,
                        const QString& MethodsToHide = {});

    static int ParseRequestTypes(const QMetaObject& Meta, const QString& Method, QString& ReturnName);
    static bool isProtected(const QMetaObject& Meta, const QString& Method);

    const QMetaObject& m_meta;
    QString        m_name;
    QString        m_version;
    HTTPMethods    m_signals;
    HTTPMethods    m_slots;
    HTTPProperties m_properties;

  private:
    Q_DISABLE_COPY(MythHTTPMetaService)
};

#endif
