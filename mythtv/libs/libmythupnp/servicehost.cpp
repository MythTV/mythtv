//////////////////////////////////////////////////////////////////////////////
// Program Name: servicehost.cpp
// Created     : Jan. 19, 2010
//
// Purpose     : Service Host Abstract Class 
//                                                                            
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#include <QDomDocument>

#include "mythlogging.h"
#include "servicehost.h"
#include "wsdl.h"
#include "xsd.h"

#define _MAX_PARAMS 256

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

MethodInfo::MethodInfo()
{
    m_nMethodIndex = 0;
    m_eRequestType = (RequestType)(RequestTypeGet | RequestTypePost |
                                   RequestTypeHead);
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QVariant MethodInfo::Invoke( Service *pService, const QStringMap &reqParams )
{
    HttpRedirectException exception;
    bool                  bExceptionThrown = false;

    if (!pService)
        throw;

    QList<QByteArray> paramNames = m_oMethod.parameterNames();
    QList<QByteArray> paramTypes = m_oMethod.parameterTypes();

    // ----------------------------------------------------------------------
    // Create Parameter array (Can't have more than _MAX_PARAMS parameters)....
    // switched to static array for performance.
    // ----------------------------------------------------------------------

    void *param[ _MAX_PARAMS ];
    int   types[ _MAX_PARAMS ];

    memset( param, 0, _MAX_PARAMS * sizeof(void *));
    memset( types, 0, _MAX_PARAMS * sizeof(int));

    try
    {
        // --------------------------------------------------------------
        // Add a place for the Return value
        // --------------------------------------------------------------

        int nRetIdx = QMetaType::type( m_oMethod.typeName() ); 

        if (nRetIdx != 0)
        {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
            param[ 0 ] = QMetaType::construct( nRetIdx );    
#else
            param[ 0 ] = QMetaType::create( nRetIdx );
#endif
            types[ 0 ] = nRetIdx;
        }
        else
        {
            param[ 0 ] = NULL;    
            types[ 0 ] = 0;
        }

        // --------------------------------------------------------------
        // Fill in parameters from request values
        // --------------------------------------------------------------

        for( int nIdx = 0; nIdx < paramNames.length(); nIdx++ )
        {
            QString sValue     = reqParams[ paramNames[ nIdx ] ];
            QString sParamType = paramTypes[ nIdx ];

            int     nId        = QMetaType::type( paramTypes[ nIdx ] );
            void   *pParam     = NULL;

            if (nId != 0)
            {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
                pParam = QMetaType::construct( nId );
#else
                pParam = QMetaType::create( nId );
#endif
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("MethodInfo::Invoke - Type unknown '%1'")
                        .arg(sParamType));
            }

            types[nIdx+1] = nId;
            param[nIdx+1] = pService->ConvertToParameterPtr( nId, sParamType,
                                                             pParam, sValue );
        }

#if 0
        QThread *currentThread = QThread::currentThread();
        QThread *objectThread  = pService->thread();

        if (currentThread == objectThread)
            LOG(VB_UPNP, LOG_DEBUG, "*** Threads are same ***");
        else
            LOG(VB_UPNP, LOG_DEBUG, "*** Threads are Different!!! ***");
#endif

        pService->qt_metacall( QMetaObject::InvokeMetaMethod, 
                               m_nMethodIndex, 
                               param );

        // --------------------------------------------------------------
        // Delete param array, skip return parameter since not dynamically
        // created.
        // --------------------------------------------------------------

        for (int nIdx=1; nIdx < paramNames.length()+1; nIdx++)
        {
            if ((types[ nIdx ] != 0) && (param[ nIdx ] != NULL))
                QMetaType::destroy( types[ nIdx ], param[ nIdx ] );
        }
    }
    catch (QString &sMsg)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MethodInfo::Invoke - An Exception Occurred: %1")
                 .arg(sMsg));

        if  ((types[ 0 ] != 0) && (param[ 0 ] != NULL ))
            QMetaType::destroy( types[ 0 ], param[ 0 ] );

        throw;
    }
    catch (HttpRedirectException &ex)
    {
        bExceptionThrown = true;
        exception = ex;
    }
    catch (...)
    {
        LOG(VB_GENERAL, LOG_INFO,
            "MethodInfo::Invoke - An Exception Occurred" );
    }

    // --------------------------------------------------------------
    // return the result after converting to a QVariant
    // --------------------------------------------------------------

    QVariant vReturn;
  
    if ( param[ 0 ] != NULL)
    {
        vReturn = pService->ConvertToVariant( types[ 0 ], param[ 0 ] );

        if  (types[ 0 ] != 0)
            QMetaType::destroy( types[ 0 ], param[ 0 ] );
    }

    // --------------------------------------------------------------
    // Re-throw exception if needed.
    // --------------------------------------------------------------

    if (bExceptionThrown)
        throw exception;

    return vReturn;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

ServiceHost::ServiceHost(const QMetaObject &metaObject,
                         const QString     &sExtensionName,
                         const QString     &sBaseUrl,
                         const QString     &sSharePath ) 
            : HttpServerExtension ( sExtensionName,   sSharePath )
{
    m_oMetaObject = metaObject;
    m_sBaseUrl    = sBaseUrl;

    // --------------------------------------------------------------
    // Read in all callable methods and cache information about them
    // --------------------------------------------------------------

    for (int nIdx = 0; nIdx < m_oMetaObject.methodCount(); nIdx++)
    {
        QMetaMethod method = m_oMetaObject.method( nIdx );

        if ((method.methodType() == QMetaMethod::Slot   ) &&
            (method.access()     == QMetaMethod::Public ))
        {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
            QString sName( method.signature() );
#else
            QString sName( method.methodSignature() );      
#endif

            // ------------------------------------------------------
            // Ignore the following methods...
            // ------------------------------------------------------

            if (sName == "deleteLater()")
                continue;

            // ------------------------------------------------------

            MethodInfo oInfo;

            oInfo.m_nMethodIndex = nIdx;
            oInfo.m_sName        = sName.section( '(', 0, 0 );
            oInfo.m_oMethod      = method;
            oInfo.m_eRequestType = (RequestType)(RequestTypeGet |
                                                 RequestTypePost |
                                                 RequestTypeHead);

            QString sMethodClassInfo = oInfo.m_sName + "_Method";

            int nClassIdx =
                m_oMetaObject.indexOfClassInfo(sMethodClassInfo.toLatin1());

            if (nClassIdx >=0)
            {
                QString sRequestType =
                    m_oMetaObject.classInfo(nClassIdx).value();

                if (sRequestType == "POST")
                    oInfo.m_eRequestType = RequestTypePost;
                else if (sRequestType == "GET" )
                    oInfo.m_eRequestType = (RequestType)(RequestTypeGet |
                                                         RequestTypeHead);
            }

            m_Methods.insert( oInfo.m_sName, oInfo );
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

ServiceHost::~ServiceHost()
{
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QStringList ServiceHost::GetBasePaths() 
{ 
    return QStringList( m_sBaseUrl );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

bool ServiceHost::ProcessRequest( HTTPRequest *pRequest )
{
    bool     bHandled = false;
    Service *pService = NULL;

    try
    {
        if (pRequest)
        {
            if (pRequest->m_sBaseUrl != m_sBaseUrl)
                return false;

            LOG(VB_UPNP, LOG_INFO,
                QString("ServiceHost::ProcessRequest: %1 : %2")
                    .arg(pRequest->m_sMethod) .arg(pRequest->m_sRawRequest));

            // --------------------------------------------------------------
            // Check to see if they are requesting the WSDL service Definition
            // --------------------------------------------------------------

            if (( pRequest->m_eType   == RequestTypeGet ) &&
                ( pRequest->m_sMethod == "wsdl"         ))
            {
                pService =  qobject_cast<Service*>(m_oMetaObject.newInstance());

                Wsdl wsdl( this );

                wsdl.GetWSDL( pRequest );

                delete pService;
                return true;
            }

            // --------------------------------------------------------------
            // Check to see if they are requesting XSD - Type Definition
            // --------------------------------------------------------------

            if (( pRequest->m_eType   == RequestTypeGet ) &&
                ( pRequest->m_sMethod == "xsd"          ))
            {
                if ( pRequest->m_mapParams.count() > 0)
                {
                    pService =  qobject_cast<Service*>(m_oMetaObject.newInstance());

                    Xsd xsd;

                    xsd.GetXSD( pRequest, pRequest->m_mapParams[ "type" ] );

                    delete pService;
                }

                return true;
            }

            if (( pRequest->m_eType   == RequestTypeGet ) &&
                ( pRequest->m_sMethod == "version"         ))
            {

                int nClassIdx = m_oMetaObject.indexOfClassInfo( "version" );

                if (nClassIdx >=0)
                {
                    QString sVersion =
                        m_oMetaObject.classInfo(nClassIdx).value();

                    return FormatResponse( pRequest, QVariant( sVersion ));
                }
            }

            // --------------------------------------------------------------
            // Allow a more REST like calling convention.  If the Method 
            // Name isn't found, search for one with the request method 
            // appended to the name ( "Get" or "Put" for POST)
            // --------------------------------------------------------------

            QString sMethodName  = pRequest->m_sMethod;
            bool    bMethodFound = false;

            if (m_Methods.contains(sMethodName))
                bMethodFound = true;
            else
            {
                switch( pRequest->m_eType )
                {
                    case RequestTypeHead:
                    case RequestTypeGet :
                        sMethodName = "Get" + sMethodName;
                        break;
                    case RequestTypePost:
                        sMethodName = "Put" + sMethodName;
                        break;
                    case RequestTypeUnknown:
                    case RequestTypeMSearch:
                    case RequestTypeSubscribe:
                    case RequestTypeUnsubscribe:
                    case RequestTypeNotify:
                    case RequestTypeResponse:
                        // silence compiler
                        break;
                }

                if (m_Methods.contains(sMethodName))
                    bMethodFound = true;
            }

            if (bMethodFound)
            {
                MethodInfo oInfo = m_Methods.value( sMethodName );

                if (( pRequest->m_eType & oInfo.m_eRequestType ) != 0)
                {
                    // ------------------------------------------------------
                    // Create new Instance of the Service Class so
                    // it's guaranteed to be on the same thread
                    // since we are making direct calls into it.
                    // ------------------------------------------------------

                    pService = 
                        qobject_cast<Service*>(m_oMetaObject.newInstance());

                    QVariant vResult = oInfo.Invoke(pService,
                                                    pRequest->m_mapParams);

                    bHandled = FormatResponse( pRequest, vResult );
                }
            }

            if (!bHandled)
                UPnp::FormatErrorResponse( pRequest, UPnPResult_InvalidAction );
        }
    }
    catch (HttpRedirectException &ex)
    {
        UPnp::FormatRedirectResponse( pRequest, ex.hostName );
        bHandled = true;
    }
    catch (HttpException &ex)
    {
        LOG(VB_GENERAL, LOG_ERR, ex.msg);
        UPnp::FormatErrorResponse( pRequest, UPnPResult_ActionFailed, ex.msg );

        bHandled = true;

    }
    catch (QString &sMsg)
    {
        LOG(VB_GENERAL, LOG_ERR, sMsg);
        UPnp::FormatErrorResponse( pRequest, UPnPResult_ActionFailed, sMsg );

        bHandled = true;
    }
    catch ( ...)
    {
        QString sMsg( "ServiceHost::ProcessRequest - Unexpected Exception" );

        LOG(VB_GENERAL, LOG_ERR, sMsg);
        UPnp::FormatErrorResponse( pRequest, UPnPResult_ActionFailed, sMsg );

        bHandled = true;
    }

    if (pService != NULL)
        delete pService;

    return bHandled;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool ServiceHost::FormatResponse( HTTPRequest *pRequest, QObject *pResults )
{
    if (pResults != NULL)
    {
        Serializer *pSer = pRequest->GetSerializer();

        pSer->Serialize( pResults );

        pRequest->FormatActionResponse( pSer );

        delete pResults;

        return true;
    }
    else
        UPnp::FormatErrorResponse( pRequest, UPnPResult_ActionFailed, "Call to method failed" );

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool ServiceHost::FormatResponse( HTTPRequest *pRequest, QFileInfo oInfo )
{
    QString sName = oInfo.absoluteFilePath();

    if (oInfo.exists())
    {
        if (oInfo.isSymLink())
            pRequest->FormatFileResponse( oInfo.symLinkTarget() );
        else
            pRequest->FormatFileResponse( oInfo.absoluteFilePath() );
    }
    else
    {
        // force return as a 404...
        pRequest->FormatFileResponse( "" );
    }

    return true;
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool ServiceHost::FormatResponse( HTTPRequest *pRequest, QVariant vValue )
{
    if ( vValue.canConvert< QObject* >()) 
    { 
        const QObject *pObject = vValue.value< QObject* >(); 

        return FormatResponse( pRequest, (QObject *)pObject );
    }

    if ( vValue.canConvert< QFileInfo >()) 
    {
        const QFileInfo oFileInfo = vValue.value< QFileInfo >(); 

        return FormatResponse( pRequest, oFileInfo );
    }

    // ----------------------------------------------------------------------
    // Simple Variant... serialize it.
    // ----------------------------------------------------------------------
    
    Serializer *pSer = pRequest->GetSerializer();

    pSer->Serialize( vValue, vValue.typeName() );

    pRequest->FormatActionResponse( pSer );

    return true;
}
