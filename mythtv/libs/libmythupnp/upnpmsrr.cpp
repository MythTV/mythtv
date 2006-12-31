/////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcds.cpp
//                                                                            
// Purpose - uPnp Microsoft Media Receiver Registrar "fake" Service 
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "upnpmsrr.h"

#include "util.h"

#include <qtextstream.h>
#include <math.h>
#include <qregexp.h>

#define DIDL_LITE_BEGIN "<DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\">"
#define DIDL_LITE_END   "</DIDL-Lite>";

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpMSRR::UPnpMSRR() : HttpServerExtension( "UPnpMSRR" )
{
    m_root.m_eType      = OT_Container;
    m_root.m_sId        = "0";
    m_root.m_sParentId  = "-1";
    m_root.m_sTitle     = "MythTv";
    m_root.m_sClass     = "object.container";
    m_root.m_bRestricted= true;
    m_root.m_bSearchable= true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpMSRR::~UPnpMSRR()
{
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpMSRRMethod UPnpMSRR::GetMethod( const QString &sURI )
{
    if (sURI == "IsAuthorized"          ) return( MSRR_IsAuthorized         );              
    if (sURI == "RegisterDevice"        ) return( MSRR_RegisterDevice       );
    if (sURI == "IsValidated"           ) return( MSRR_IsValidated          ); 

    return(  MSRR_Unknown );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpMSRR::ProcessRequest( HttpWorkerThread * /*pThread*/, HTTPRequest *pRequest )
{
    VERBOSE(VB_UPNP, QString("UPnpMSRR::ProcessRequest : %1 : %2 :").arg(pRequest->m_sBaseUrl).arg(pRequest->m_sMethod));

    if (pRequest)
    {
        if ( pRequest->m_sBaseUrl != "/_MSRR_1-0_control" )
            return false;

        switch( GetMethod( pRequest->m_sMethod ) )
        {
            case MSRR_IsAuthorized               : HandleIsAuthorized         ( pRequest ); break;
            case MSRR_RegisterDevice             : HandleRegisterDevice       ( pRequest ); break;
            case MSRR_IsValidated                : HandleIsValidated          ( pRequest ); break;

            default:
                pRequest->FormatErrorReponse( 401, "Invalid Action" );
                pRequest->m_nResponseStatus = 501;
                break;
        }       
    }

    return( true );

}

void UPnpMSRR::HandleIsAuthorized( HTTPRequest *pRequest )
{
    /* Always tell the user they are authorized to access this data */
    VERBOSE(VB_UPNP, QString("UPnpMSRR::HandleIsAuthorized"));
    NameValueList list;

    list.append( new NameValue( "Result"        , "1"));

    pRequest->FormatActionReponse( &list );

}

void UPnpMSRR::HandleRegisterDevice( HTTPRequest *pRequest )
{
    /* Sure, register, we don't really care */
    VERBOSE(VB_UPNP, QString("UPnpMSRR::HandleRegisterDevice"));
    NameValueList list;

    list.append( new NameValue( "Result"        , "1"));

    pRequest->FormatActionReponse( &list );

}

void UPnpMSRR::HandleIsValidated( HTTPRequest *pRequest )
{
    /* You are valid sir */

    VERBOSE(VB_UPNP, QString("UPnpMSRR::HandleIsValidated"));
    NameValueList list;

    list.append( new NameValue( "Result"        , "1"));

    pRequest->FormatActionReponse( &list );

}

//
/////////////////////////////////////////////////////////////////////////////

QString &UPnpMSRR::Encode( QString &sStr )
{
    sStr.replace(QRegExp( "&"), "&amp;" ); // This _must_ come first
    sStr.replace(QRegExp( "<"), "&lt;"  );
    sStr.replace(QRegExp( ">"), "&gt;"  );
//    sStr.replace(QRegExp("\""), "&quot;");
//    sStr.replace(QRegExp( "'"), "&apos;");

    return( sStr );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////



