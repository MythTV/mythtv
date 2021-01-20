#ifndef MYTHWSDL_H
#define MYTHWSDL_H

// Qt
#include <QMap>
#include <QDomDocument>

// MythTV
#include "http/mythhttptypes.h"
#include "http/mythxsd.h"

class MythHTTPMetaService;

class MythWSDL : public QDomDocument
{
  public:
    static HTTPResponse GetWSDL(HTTPRequest2 Request, MythHTTPMetaService* MetaService);

  protected:
    MythWSDL(HTTPRequest2 Request, MythHTTPMetaService* MetaService);
    HTTPResponse m_result { nullptr };
    QMap<QString, TypeInfo> m_typesToInclude;
    QDomElement             m_oRoot;
    QDomElement             m_oTypes;
    QDomElement             m_oLastMsg;
    QDomElement             m_oPortType;
    QDomElement             m_oBindings;
    QDomElement             m_oService;
};

#endif
