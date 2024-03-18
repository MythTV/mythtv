//////////////////////////////////////////////////////////////////////////////
// Program Name: mythxml.h
//                                                                            
// Purpose - Myth Frontend XML protocol HttpServerExtension 
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef MYTHFEXML_H_
#define MYTHFEXML_H_

#include "libmyth/mythcontext.h"
#include "libmythupnp/eventing.h"
#include "libmythupnp/upnp.h"

enum MythFEXMLMethod : std::uint8_t
{
    MFEXML_Unknown = 0,
    MFEXML_GetServiceDescription,
    MFEXML_GetScreenShot,
    MFEXML_ActionListTest,
    MFEXML_GetRemote,
};

class MythFEXML : public Eventing
{
  private:

    QString m_sControlUrl;
    QString m_sServiceDescFileName;

  protected:

    // Implement UPnpServiceImpl methods that we can

    QString GetServiceType() override // UPnpServiceImpl
        { return "urn:schemas-mythtv-org:service:MythFrontend:1"; }
    QString GetServiceId() override // UPnpServiceImpl
        { return "urn:mythtv-org:serviceId:MYTHFRONTEND_1-0"; }
    QString GetServiceControlURL() override // UPnpServiceImpl
        { return m_sControlUrl.mid( 1 ); }
    QString GetServiceDescURL() override // UPnpServiceImpl
        { return m_sControlUrl.mid( 1 ) + "/GetServDesc"; }

  private:

    static MythFEXMLMethod GetMethod( const QString &sURI );

    static void GetScreenShot    ( HTTPRequest *pRequest );
    static void GetActionListTest( HTTPRequest *pRequest );
    static void GetRemote        ( HTTPRequest *pRequest );

  public:
    MythFEXML( UPnpDevice *pDevice ,  const QString &sSharePath);
    ~MythFEXML() override = default;

    QStringList GetBasePaths() override; // Eventing

    bool ProcessRequest( HTTPRequest *pRequest ) override; // Eventing
};

#endif


