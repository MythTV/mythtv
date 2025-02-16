//////////////////////////////////////////////////////////////////////////////
// Program Name: servicehost.cpp
// Created     : Jan. 19, 2010
//
// Purpose     : Service Host Abstract Class
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
#include "servicehost.h"

#include "libmythbase/mythlogging.h"

#include "libmythupnp/upnp.h"
#include "wsdl.h"
#include "xsd.h"

static constexpr int MAX_PARAMS = 256;

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QVariant MethodInfo::Invoke( Service *pService, const QStringMap &reqParams ) const
{
    HttpRedirectException exception;
    bool                  bExceptionThrown = false;
    QStringMap            lowerParams;

    if (!pService)
        throw QString("Invalid argument to MethodInfo::Invoke. pService in nullptr");

     // Change params to lower case for case-insensitive comparison
    for (auto it = reqParams.cbegin(); it != reqParams.cend(); ++it)
    {
        lowerParams[it.key().toLower()] = *it;
    }

    // --------------------------------------------------------------
    // Provide actual parameters received to method
    // --------------------------------------------------------------

    pService->m_parsedParams = lowerParams.keys();


    QList<QByteArray> paramNames = m_oMethod.parameterNames();
    QList<QByteArray> paramTypes = m_oMethod.parameterTypes();

    // ----------------------------------------------------------------------
    // Create Parameter array (Can't have more than MAX_PARAMS parameters)....
    // switched to static array for performance.
    // ----------------------------------------------------------------------

    std::array<void*,MAX_PARAMS> param {};
    std::array<int,MAX_PARAMS> types {};

    try
    {
        // --------------------------------------------------------------
        // Add a place for the Return value
        // --------------------------------------------------------------

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        int nRetIdx = QMetaType::type( m_oMethod.typeName() );
        QMetaType oRetType = QMetaType(nRetIdx);
#else
        int nRetIdx = m_oMethod.returnType();
        QMetaType oRetType = m_oMethod.returnMetaType();
#endif

        if (nRetIdx != QMetaType::UnknownType)
        {
            param[ 0 ] = oRetType.create();
            types[ 0 ] = nRetIdx;
        }
        else
        {
            param[ 0 ] = nullptr;
            types[ 0 ] = QMetaType::UnknownType;
        }

        // --------------------------------------------------------------
        // Fill in parameters from request values
        // --------------------------------------------------------------

        for( int nIdx = 0; nIdx < paramNames.length(); ++nIdx )
        {
            QString sValue     = lowerParams[ paramNames[ nIdx ].toLower() ];
            QString sParamType = paramTypes[ nIdx ];

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            int     nId        = QMetaType::type( paramTypes[ nIdx ] );
            void   *pParam     = nullptr;

            if (nId != QMetaType::UnknownType)
            {
                pParam = QMetaType::create( nId );
            }
#else
            QMetaType metaType = QMetaType::fromName( paramTypes[ nIdx ] );
            void *pParam = metaType.create();
            int nId = metaType.id();
#endif
            if (nId == QMetaType::UnknownType)
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("MethodInfo::Invoke - Type unknown '%1'")
                        .arg(sParamType));
                throw QString("MethodInfo::Invoke - Type unknown '%1'")
                          .arg(sParamType);
            }

            types[nIdx+1] = nId;
            param[nIdx+1] = pService->ConvertToParameterPtr( nId, sParamType,
                                                             pParam, sValue );
        }

#if 0
        QThread *currentThread = QThread::currentThread();
        QThread *objectThread  = pService->thread();

        if (currentThread == objectThread)
            LOG(VB_HTTP, LOG_DEBUG, "*** Threads are same ***");
        else
            LOG(VB_HTTP, LOG_DEBUG, "*** Threads are Different!!! ***");
#endif

        if (pService->qt_metacall(QMetaObject::InvokeMetaMethod, m_nMethodIndex, param.data()) >= 0)
            LOG(VB_GENERAL, LOG_WARNING, "qt_metacall error");

        // --------------------------------------------------------------
        // Delete param array, skip return parameter since not dynamically
        // created.
        // --------------------------------------------------------------

        for (int nIdx=1; nIdx < paramNames.length()+1; ++nIdx)
        {
            if ((types[ nIdx ] != QMetaType::UnknownType) && (param[ nIdx ] != nullptr))
            {
                auto metaType = QMetaType( types[ nIdx ] );
                metaType.destroy(param[ nIdx ]);
            }
        }
    }
    catch (QString &sMsg)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MethodInfo::Invoke - An Exception Occurred: %1")
                 .arg(sMsg));

        if  ((types[ 0 ] != QMetaType::UnknownType) && (param[ 0 ] != nullptr ))
        {
            auto metaType = QMetaType( types[ 0 ] );
            metaType.destroy(param[ 0 ]);
        }

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

    if ( param[ 0 ] != nullptr)
    {
        vReturn = pService->ConvertToVariant( types[ 0 ], param[ 0 ] );

        if (types[ 0 ] != QMetaType::UnknownType)
        {
            auto metaType = QMetaType( types[ 0 ] );
            metaType.destroy(param[ 0 ]);
        }
    }

    // --------------------------------------------------------------
    // Re-throw exception if needed.
    // --------------------------------------------------------------

    if (bExceptionThrown)
        throw HttpRedirectException(exception);

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
                               QString      sBaseUrl,
                         const QString     &sSharePath )
            : HttpServerExtension ( sExtensionName,   sSharePath ),
              m_sBaseUrl(std::move(sBaseUrl)),
              m_oMetaObject(metaObject)
{
    // ----------------------------------------------------------------------
    // Create an instance of the service so custom types get registered.
    // ----------------------------------------------------------------------

    QObject *pService =  m_oMetaObject.newInstance();

    // ----------------------------------------------------------------------
    // Read in all callable methods and cache information about them
    // ----------------------------------------------------------------------

    for (int nIdx = 0; nIdx < m_oMetaObject.methodCount(); nIdx++)
    {
        QMetaMethod method = m_oMetaObject.method( nIdx );

        if ((method.methodType() == QMetaMethod::Slot   ) &&
            (method.access()     == QMetaMethod::Public ))
        {
            QString sName( method.methodSignature() );

            // --------------------------------------------------------------
            // Ignore the following methods...
            // --------------------------------------------------------------

            if (sName == "deleteLater()")
                continue;

            // --------------------------------------------------------------

            MethodInfo oInfo;

            oInfo.m_nMethodIndex = nIdx;
            oInfo.m_sName        = sName.section( '(', 0, 0 );
            oInfo.m_oMethod      = method;
            oInfo.m_eRequestType = (HttpRequestType)(RequestTypeGet |
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
                    oInfo.m_eRequestType = (HttpRequestType)(RequestTypeGet |
                                                             RequestTypeHead);
            }

            m_methods.insert( oInfo.m_sName, oInfo );
        }
    }

    // ----------------------------------------------------------------------

    delete pService;
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
    Service *pService = nullptr;

    try
    {
        if (pRequest)
        {
            if (pRequest->m_sBaseUrl != m_sBaseUrl)
                return false;

            LOG(VB_HTTP, LOG_INFO,
                QString("ServiceHost::ProcessRequest: %1 : %2")
                    .arg(pRequest->m_sMethod, pRequest->m_sRawRequest));

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
                bool bHandled2 = false;
                if ( pRequest->m_mapParams.count() > 0)
                {
                    pService =  qobject_cast<Service*>(m_oMetaObject.newInstance());

                    Xsd xsd;

                    if (pRequest->m_mapParams.contains( "type" ))
                        bHandled2 = xsd.GetXSD( pRequest, pRequest->m_mapParams[ "type" ] );
                    else
                        bHandled2 = xsd.GetEnumXSD( pRequest, pRequest->m_mapParams[ "enum" ] );
                    delete pService;
                    pService = nullptr;
                }

                if (!bHandled2)
                    throw QString("Invalid arguments to xsd query: %1")
                        .arg(pRequest->m_sRequestUrl.section('?', 1));

                return true;
            }

            // --------------------------------------------------------------

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

            if (m_methods.contains(sMethodName))
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
                    case RequestTypeOptions:
                    case RequestTypeMSearch:
                    case RequestTypeSubscribe:
                    case RequestTypeUnsubscribe:
                    case RequestTypeNotify:
                    case RequestTypeResponse:
                        // silence compiler
                        break;
                }

                if (m_methods.contains(sMethodName))
                    bMethodFound = true;
            }

            if (bMethodFound)
            {
                MethodInfo oInfo = m_methods.value( sMethodName );

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
        UPnp::FormatRedirectResponse( pRequest, ex.m_hostName );
        bHandled = true;
    }
    catch (HttpException &ex)
    {
        LOG(VB_GENERAL, LOG_ERR, ex.m_msg);
        UPnp::FormatErrorResponse( pRequest, UPnPResult_ActionFailed, ex.m_msg );

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

    delete pService;
    return bHandled;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool ServiceHost::FormatResponse( HTTPRequest *pRequest, QObject *pResults )
{
    if (pResults != nullptr)
    {
        Serializer *pSer = pRequest->GetSerializer();

        pSer->Serialize( pResults );

        pRequest->FormatActionResponse( pSer );

        delete pResults;

        return true;
    }
    UPnp::FormatErrorResponse( pRequest, UPnPResult_ActionFailed, "Call to method failed" );

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool ServiceHost::FormatResponse( HTTPRequest *pRequest, const QFileInfo& oInfo )
{
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

bool ServiceHost::FormatResponse( HTTPRequest *pRequest, const QVariant& vValue )
{
    if ( vValue.canConvert< QObject* >())
    {
        const QObject *pObject = vValue.value< QObject* >();

        return FormatResponse( pRequest, (QObject *)pObject );
    }

    if ( vValue.canConvert< QFileInfo >())
    {
        const auto oFileInfo = vValue.value< QFileInfo >();

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
