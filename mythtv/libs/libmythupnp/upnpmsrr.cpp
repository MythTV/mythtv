/////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcds.cpp
//                                                                            
// Purpose - uPnp Microsoft Media Receiver Registrar "fake" Service 
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "upnpmsrr.h"

#include <math.h>
#include <qregexp.h>

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpMSRR::UPnpMSRR() : Eventing( "UPnpMSRR", "_MSRR_1-0_event" )
{
    AddVariable( new StateVariable< unsigned short >( "AuthorizationGrantedUpdateID", true ) );
    AddVariable( new StateVariable< unsigned short >( "AuthorizationDeniedUpdateID" , true ) );
    AddVariable( new StateVariable< unsigned short >( "ValidationSucceededUpdateID" , true ) );
    AddVariable( new StateVariable< unsigned short >( "ValidationRevokedUpdateID"   , true ) );

    SetValue< unsigned short >( "AuthorizationGrantedUpdateID", 0 );
    SetValue< unsigned short >( "AuthorizationDeniedUpdateID" , 0 );
    SetValue< unsigned short >( "ValidationSucceededUpdateID" , 0 );
    SetValue< unsigned short >( "ValidationRevokedUpdateID"   , 0 );
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

bool UPnpMSRR::ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest )
{
    if (pRequest)
    {
        if (Eventing::ProcessRequest( pThread, pRequest ))
            return true;

        if ( pRequest->m_sBaseUrl != "/_MSRR_1-0_control" )
            return false;

        VERBOSE(VB_UPNP, QString("UPnpMSRR::ProcessRequest : %1 : %2 :")
                            .arg( pRequest->m_sBaseUrl )
                            .arg( pRequest->m_sMethod  ));

        switch( GetMethod( pRequest->m_sMethod ) )
        {
            case MSRR_IsAuthorized               : HandleIsAuthorized         ( pRequest ); break;
            case MSRR_RegisterDevice             : HandleRegisterDevice       ( pRequest ); break;
            case MSRR_IsValidated                : HandleIsValidated          ( pRequest ); break;

            default:
                pRequest->FormatErrorReponse( 401, "Invalid Action" );
                pRequest->m_nResponseStatus = 401; //501;
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

