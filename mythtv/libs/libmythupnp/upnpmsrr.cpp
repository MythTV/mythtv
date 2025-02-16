/////////////////////////////////////////////////////////////////////////////
// Program Name: upnpmsrr.cpp
//                                                                            
// Purpose - uPnp Microsoft Media Receiver Registrar "fake" Service 
//                                                                            
//////////////////////////////////////////////////////////////////////////////
#include "upnpmsrr.h"

#include "libmythbase/configuration.h"
#include "libmythbase/mythlogging.h"

#include "upnp.h"
#include "upnputil.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpMSRR::UPnpMSRR( UPnpDevice *pDevice, const QString &sSharePath ) 
               : Eventing( "UPnpMSRR", "MSRR_Event", sSharePath),
                 m_sControlUrl("/MSRR_Control")
{
    AddVariable(
        new StateVariable<unsigned short>("AuthorizationGrantedUpdateID",
                                          true));
    AddVariable(
        new StateVariable<unsigned short>("AuthorizationDeniedUpdateID", true));
    AddVariable(
        new StateVariable<unsigned short>("ValidationSucceededUpdateID", true));
    AddVariable(
        new StateVariable<unsigned short>("ValidationRevokedUpdateID", true));

    SetValue<unsigned short>("AuthorizationGrantedUpdateID", 0);
    SetValue<unsigned short>("AuthorizationDeniedUpdateID" , 0);
    SetValue<unsigned short>("ValidationSucceededUpdateID" , 0);
    SetValue<unsigned short>("ValidationRevokedUpdateID"   , 0);

    QString sUPnpDescPath = XmlConfiguration().GetValue("UPnP/DescXmlPath", m_sSharePath);

    m_sServiceDescFileName = sUPnpDescPath + "MSRR_scpd.xml";

    // Add our Service Definition to the device.
    RegisterService( pDevice );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpMSRRMethod UPnpMSRR::GetMethod( const QString &sURI )
{
    if (sURI == "GetServDesc" )
        return MSRR_GetServiceDescription;
    if (sURI == "IsAuthorized" )
        return MSRR_IsAuthorized;
    if (sURI == "RegisterDevice" )
        return MSRR_RegisterDevice;
    if (sURI == "IsValidated" )
        return MSRR_IsValidated;

    return MSRR_Unknown;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList UPnpMSRR::GetBasePaths() 
{ 
    return Eventing::GetBasePaths() << m_sControlUrl; 
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpMSRR::ProcessRequest( HTTPRequest *pRequest )
{
    if (pRequest)
    {
        if (Eventing::ProcessRequest( pRequest ))
            return true;

        if ( pRequest->m_sBaseUrl != m_sControlUrl )
            return false;

        LOG(VB_UPNP, LOG_INFO,
            QString("UPnpMSRR::ProcessRequest : %1 : %2 :")
                .arg(pRequest->m_sBaseUrl, pRequest->m_sMethod));

        switch(GetMethod(pRequest->m_sMethod))
        {
            case MSRR_GetServiceDescription :
                pRequest->FormatFileResponse( m_sServiceDescFileName );
                break;
            case MSRR_IsAuthorized :
                HandleIsAuthorized( pRequest );
                break;
            case MSRR_RegisterDevice :
                HandleRegisterDevice( pRequest );
                break;
            case MSRR_IsValidated :
                HandleIsValidated( pRequest );
                break;
            default:
                UPnp::FormatErrorResponse( pRequest, UPnPResult_InvalidAction );
                break;
        }       
    }

    return( true );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpMSRR::HandleIsAuthorized( HTTPRequest *pRequest )
{
    /* Always tell the user they are authorized to access this data */
    LOG(VB_UPNP, LOG_DEBUG, "UPnpMSRR::HandleIsAuthorized");
    NameValues list;

    list.push_back(NameValue("Result", "1"));

    list.back().AddAttribute("xmlns:dt", "urn:schemas-microsoft-com:datatypes", true);
    list.back().AddAttribute("dt:dt",    "int", true);

    pRequest->FormatActionResponse(list);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpMSRR::HandleRegisterDevice( HTTPRequest *pRequest )
{
    /* Sure, register, we don't really care */
    LOG(VB_UPNP, LOG_DEBUG, "UPnpMSRR::HandleRegisterDevice");
    NameValues list;

    list.push_back(NameValue("Result", "1"));

    pRequest->FormatActionResponse(list);
}

void UPnpMSRR::HandleIsValidated( HTTPRequest *pRequest )
{
    /* You are valid sir */
    LOG(VB_UPNP, LOG_DEBUG, "UPnpMSRR::HandleIsValidated");
    NameValues list;

    list.push_back(NameValue("Result", "1"));

    list.back().AddAttribute("xmlns:dt", "urn:schemas-microsoft-com:datatypes", true);
    list.back().AddAttribute( "dt:dt",   "int", true);

    pRequest->FormatActionResponse(list);
}

