//////////////////////////////////////////////////////////////////////////////
// Program Name: eventing.cpp
//                                                                            
// Purpose - uPnp Eventing Base Class Implementation
//                                                                            
// Created By  : David Blain                    Created On : Dec. 22, 2006
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "upnp.h"
#include "eventing.h"
#include "upnptaskevent.h"

#include "util.h"

#include <qtextstream.h>
#include <math.h>
#include <qregexp.h>

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Eventing::Eventing( const QString &sExtensionName, const QString &sEventMethodName ) : HttpServerExtension( sExtensionName )
{
    m_sEventMethodName      = sEventMethodName;
    m_nSubscriptionDuration = UPnp::g_pConfig->GetValue( "UPnP/SubscriptionDuration", 1800 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Eventing::~Eventing()
{
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

inline short Eventing::HoldEvents()
{
    // -=>TODO: Should use an Atomic increment... 
    //          need to research available functions.

    short nVal;

    m_mutex.lock();
    nVal = (m_nHoldCount++);
    m_mutex.unlock();

    return nVal;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

inline short Eventing::ReleaseEvents()
{
    // -=>TODO: Should use an Atomic decrement... 

    short nVal;

    m_mutex.lock();
    nVal = (m_nHoldCount--);
    m_mutex.unlock();

    if (nVal == 0)
        Notify();

    return nVal;
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Eventing::ProcessRequest( HttpWorkerThread * /*pThread*/, HTTPRequest *pRequest )
{
    if (pRequest)
    {
        if ( pRequest->m_sBaseUrl != "/" )
            return false;

        if ( pRequest->m_sMethod != m_sEventMethodName )
            return false;

        VERBOSE( VB_UPNP, QString("Eventing::ProcessRequest - Method (%1)").arg(pRequest->m_sMethod ));

        switch( pRequest->m_eType )
        {
            case RequestTypeSubscribe   : HandleSubscribe     ( pRequest ); break;
            case RequestTypeUnsubscribe : HandleUnsubscribe   ( pRequest ); break;
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

void Eventing::ExecutePostProcess( )
{
    // Use PostProcessing Hook to perform Initial Notification
    // to make sure they receive it AFTER the subscription results

    if (m_pInitializeSubscriber != NULL)
    {
        NotifySubscriber( m_pInitializeSubscriber );
        
        m_pInitializeSubscriber = NULL;
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void Eventing::HandleSubscribe( HTTPRequest *pRequest ) 
{
    pRequest->m_eResponseType   = ResponseTypeXML;
    pRequest->m_nResponseStatus = 412;

    QString sCallBack = pRequest->GetHeaderValue( "CALLBACK", "" );
    QString sNT       = pRequest->GetHeaderValue( "NT"      , "" );
    QString sTimeout  = pRequest->GetHeaderValue( "TIMOUT"  , "" );
    QString sSID      = pRequest->GetHeaderValue( "SID"     , "" );

    SubscriberInfo *pInfo = NULL;

    // ----------------------------------------------------------------------
    // Validate Header Values...
    // ----------------------------------------------------------------------

    // -=>TODO: Need to add support for more than one CallBack URL.
    // -=>TODO: Need to handle Timeout header

    if ( sCallBack.length() != 0 )
    {
        // ------------------------------------------------------------------
        // New Subscription
        // ------------------------------------------------------------------

        if ( sSID.length() != 0 )   
        { 
            pRequest->m_nResponseStatus = 400; 
            return; 
        }

        if ( sNT != "upnp:event" )   
            return;

        // ----------------------------------------------------------------------
        // Process Subscription
        // ----------------------------------------------------------------------

        // -=>TODO: Temp code until support for multiple callbacks are supported.

        sCallBack = sCallBack.mid( 1, sCallBack.find( ">" ) - 1);

        pInfo = new SubscriberInfo( sCallBack, m_nSubscriptionDuration );

        m_Subscribers.insert( pInfo->sUUID, pInfo );

        // Use PostProcess Hook to Send Initial FULL Notification...
        //      *** Must send this response first then notify.

        m_pInitializeSubscriber = pInfo;
        pRequest->m_pPostProcess   = (IPostProcess *)this;

    }
    else
    {
        // ------------------------------------------------------------------
        // Renewal
        // ------------------------------------------------------------------

        if ( sSID.length() != 0 )   
        {
            sSID  = sSID.mid( 5 );
            pInfo = m_Subscribers.find( sSID );
        }

    }
    
    if (pInfo != NULL)
    {
        pRequest->m_mapRespHeaders[ "SID"    ] = QString( "uuid:%1" )
                                                    .arg( pInfo->sUUID );

        pRequest->m_mapRespHeaders[ "TIMEOUT"] = QString( "Second-%1" )
                                                    .arg( pInfo->nDuration );

        pRequest->m_nResponseStatus = 200;

    }

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void Eventing::HandleUnsubscribe( HTTPRequest *pRequest ) 
{
    pRequest->m_eResponseType   = ResponseTypeXML;
    pRequest->m_nResponseStatus = 412;

    QString sCallBack = pRequest->GetHeaderValue( "CALLBACK", "" );
    QString sNT       = pRequest->GetHeaderValue( "NT"      , "" );
    QString sSID      = pRequest->GetHeaderValue( "SID"     , "" );

    if ((sCallBack.length() != 0) || (sNT.length() != 0))     
    {
        pRequest->m_nResponseStatus = 400;
        return;
    }

    sSID = sSID.mid( 5 );

    if (!m_Subscribers.remove( sSID ))
        return;

    pRequest->m_nResponseStatus = 200;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void Eventing::Notify()
{
    TaskTime tt;
    gettimeofday( &tt, NULL );

    m_mutex.lock();


    for ( SubscriberIterator it( m_Subscribers ); it.current();  )
    { 
        SubscriberInfo *pInfo = it.current();

        if ( pInfo != NULL )
        {
            // --------------------------------------------------------------
            // Check to see if it's time to expire this Subscription.
            // --------------------------------------------------------------

            if ( tt < pInfo->ttExpires )
            {
                // --------------------------------------------------------------
                // Nope... Send Event notification
                // --------------------------------------------------------------

                NotifySubscriber( pInfo );

                ++it;
            }
            else
            {
                // --------------------------------------------------------------
                // Yes... Remove subscriber from list 
                //        (automatically moves iterator to next item)
                // --------------------------------------------------------------

                m_Subscribers.remove( pInfo->sUUID );
            }
        }
    }

    m_mutex.unlock();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void Eventing::NotifySubscriber( SubscriberInfo *pInfo )
{
    if (pInfo == NULL)
        return;

    int          nCount  = 0;
    QByteArray   aBody;
    QTextStream  tsBody( aBody, IO_WriteOnly );

    tsBody.setEncoding( QTextStream::UnicodeUTF8 );

    // ----------------------------------------------------------------------
    // Build Body... Only send if there are changes
    // ----------------------------------------------------------------------

    if (( nCount = BuildNotifyBody( tsBody, pInfo->ttLastNotified )) > 0)
    {

        // -=>TODO: Need to add support for more than one CallBack URL.

        QByteArray  *pBuffer = new QByteArray();    // UPnpEventTask will delete this pointer.
        QTextStream  tsMsg( *pBuffer, IO_WriteOnly );

        tsMsg.setEncoding( QTextStream::UnicodeUTF8 );

        // ----------------------------------------------------------------------
        // Build Message Header 
        // ----------------------------------------------------------------------

        short   nPort = pInfo->qURL.hasPort() ? pInfo->qURL.port() : 80;
        QString sHost = QString( "%1:%2" ).arg( pInfo->qURL.host() )
                                          .arg( nPort );

        tsMsg << "NOTIFY " << pInfo->qURL.path() << " HTTP/1.1\r\n";
        tsMsg << "HOST: " << sHost << "\r\n";
        tsMsg << "CONTENT-TYPE: \"text/xml\"\r\n";
        tsMsg << "Content-Length: " << QString::number( aBody.size() ) << "\r\n";
        tsMsg << "NT: upnp:event\r\n";
        tsMsg << "NTS: upnp:propchange\r\n";
        tsMsg << "SID: uuid:" << pInfo->sUUID << "\r\n";
        tsMsg << "SEQ: " << QString::number( pInfo->nKey ) << "\r\n";
        tsMsg << "\r\n";
        tsMsg.writeRawBytes( aBody.data(), aBody.size() );

        // ------------------------------------------------------------------
        // Add new EventTask to the TaskQueue to do the actual sending.
        // ------------------------------------------------------------------

        VERBOSE(VB_UPNP, QString("UPnp::Eventing::NotifySubscriber( %1 ) : %2 Variables").arg( sHost ).arg(nCount));

        UPnpEventTask    *pEventTask      = new UPnpEventTask( pInfo->qURL.host(), nPort, pBuffer ); 

        UPnp::g_pTaskQueue->AddTask( 250, pEventTask );

        // ------------------------------------------------------------------
        // Update the subscribers Key & last Notified fields
        // ------------------------------------------------------------------

        pInfo->IncrementKey();

        gettimeofday( &pInfo->ttLastNotified, NULL );
    }

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int Eventing::BuildNotifyBody( QTextStream &ts, TaskTime ttLastNotified )
{
    int nCount = 0;

    ts << "<?xml version=\"1.0\"?>" << endl
       << "<e:propertyset xmlns:e=\"urn:schemas-upnp-org:event-1-0\">" << endl;

    for( StateVariableIterator it( *((StateVariables *)this) ); it.current(); ++it )
    {
        StateVariableBase *pBase = it.current();

        if ( ttLastNotified < pBase->m_ttLastChanged )
        {
            nCount++;

            ts << "<e:property>" << endl;
            ts <<   "<" << pBase->m_sName << ">";
            ts <<     pBase->ToString();
            ts <<   "</" << pBase->m_sName << ">";
            ts << "</e:property>" << endl;
        }
    }

    ts << "</e:propertyset>" << endl;

    return nCount;
}

