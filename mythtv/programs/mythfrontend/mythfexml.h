//////////////////////////////////////////////////////////////////////////////
// Program Name: mythxml.h
//                                                                            
// Purpose - Myth Frontend XML protocol HttpServerExtension 
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef MYTHFEXML_H_
#define MYTHFEXML_H_

#include <QDateTime>

#include "upnp.h"
#include "eventing.h"
#include "mythcontext.h"

typedef enum 
{
    MFEXML_Unknown = 0,
    MFEXML_GetServiceDescription,
    MFEXML_GetScreenShot,
    MFEXML_Message,
    MFEXML_Action,
    MFEXML_ActionList,
    MFEXML_ActionListTest,
    MFEXML_GetRemote,
} MythFEXMLMethod;

class MythFEXML : public Eventing
{
  private:

    QString m_sControlUrl;
    QString m_sServiceDescFileName;

    QStringList m_actionList;
    QHash<QString,QStringList> m_actionDescriptions;

  protected:

    // Implement UPnpServiceImpl methods that we can

    virtual QString GetServiceType      () { return "urn:schemas-mythtv-org:service:MythFrontend:1"; }
    virtual QString GetServiceId        () { return "urn:mythtv-org:serviceId:MYTHFRONTEND_1-0"; }
    virtual QString GetServiceControlURL() { return m_sControlUrl.mid( 1 ); }
    virtual QString GetServiceDescURL   () { return m_sControlUrl.mid( 1 ) + "/GetServDesc"; }

  private:

    MythFEXMLMethod GetMethod( const QString &sURI );

    void GetScreenShot    ( HTTPRequest *pRequest );
    void SendMessage      ( HTTPRequest *pRequest );
    void SendAction       ( HTTPRequest *pRequest );
    void GetActionList    ( HTTPRequest *pRequest );
    void GetActionListTest( HTTPRequest *pRequest );
    void GetRemote        ( HTTPRequest *pRequest );
    bool IsValidAction    ( const QString &action );
    void InitActions      ( void );

  public:
    MythFEXML( UPnpDevice *pDevice ,  const QString sSharePath);
    virtual ~MythFEXML();

    virtual QStringList GetBasePaths();

    bool ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );

    // Static methods shared with HttpStatus

};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// 
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
#endif


