#ifndef MYTHHTTPSERVICES_H
#define MYTHHTTPSERVICES_H

// MythTV
#include "libmythbase/http/mythhttpservice.h"

class MythHTTPServices : public MythHTTPService
{
    friend class MythHTTPSocket;

    Q_OBJECT
    Q_CLASSINFO("Version", "1.0.0")
    Q_PROPERTY(QStringList ServiceList READ GetServiceList NOTIFY ServiceListChanged MEMBER m_serviceList)

  signals:
    void ServiceListChanged(const QStringList& ServiceList);

  public slots:
    QStringList GetServiceList();

  public:
    MythHTTPServices();
   ~MythHTTPServices() override = default;

  protected slots:
    void UpdateServices(const HTTPServices& Services);

  protected:
    QStringList m_serviceList;

  private:
    Q_DISABLE_COPY(MythHTTPServices)
};

#endif

