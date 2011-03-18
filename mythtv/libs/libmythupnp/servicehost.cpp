//////////////////////////////////////////////////////////////////////////////
// Program Name: servicehost.cpp
// Created     : Jan. 19, 2010
//
// Purpose     : Service Host Abstract Class 
//                                                                            
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include <QDomDocument>

#include "mythverbose.h"
#include "servicehost.h"
#include "wsdl.h"

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

MethodInfo::MethodInfo()
{
    m_eRequestType = (RequestType)(RequestTypeGet | RequestTypePost);
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QVariant MethodInfo::Invoke( Service *pService, const QStringMap &reqParams )
{
    QVariant vResult;

    void *pResult = NULL; 

    if (!pService)
        throw;

    QList<QByteArray> paramNames = m_oMethod.parameterNames();
    QList<QByteArray> paramTypes = m_oMethod.parameterTypes();

    // --------------------------------------------------------------
    // Create Parameter array....
    // --------------------------------------------------------------

    void **param = new void*[ paramNames.length()+1 ];

    // --------------------------------------------------------------
    // Add a place for the Return value
    // --------------------------------------------------------------

    param[ 0 ] = &pResult;    

    // --------------------------------------------------------------
    // Fill in parameters from request values
    // --------------------------------------------------------------

    for( int nIdx = 0; nIdx < paramNames.length(); nIdx++ )
    {
        QString sValue     = reqParams[ paramNames[ nIdx ] ];
        QString sParamType = paramTypes[ nIdx ];

        param[nIdx+1] = pService->ConvertToParameterPtr( sParamType, sValue );
    }

    /* ******************************** 
    QThread *currentThread = QThread::currentThread();
    QThread *objectThread  = pService->thread();

    if (currentThread == objectThread)
        VERBOSE( VB_UPNP, "*** Threads are same ***" );
    else
        VERBOSE( VB_UPNP, "*** Threads are Different!!! ***" );
     ******************************** */

    bool bSuccess = pService->qt_metacall( QMetaObject::InvokeMetaMethod, 
                                           m_nMethodIndex, 
                                           param );

    // --------------------------------------------------------------
    // Delete param array, skip return parameter since not dynamically created.
    // --------------------------------------------------------------

    for (int nIdx=1; nIdx < paramNames.length()+1; nIdx++)
    {
        if (param[ nIdx ] != NULL)
            delete param[ nIdx ];
    }

    if (param)
        delete[] param;

    // --------------------------------------------------------------
    // return the result after converting to a QVariant
    // --------------------------------------------------------------

    return pService->ConvertToVariant( m_oMethod.typeName(), pResult );
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

ServiceHost::ServiceHost( const QMetaObject &metaObject, const QString &sExtensionName,
                          const QString     &sBaseUrl  , const QString &sSharePath ) 
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
            QString sName( method.signature() );

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
            oInfo.m_eRequestType = (RequestType)(RequestTypeGet | RequestTypePost);

            QString sMethodClassInfo = oInfo.m_sName + "_Method";

            int nClassIdx = m_oMetaObject.indexOfClassInfo( sMethodClassInfo.toAscii() );

            if (nClassIdx >=0)
            {
                QString sRequestType = m_oMetaObject.classInfo( nClassIdx ).value();

                if (sRequestType == "POST")
                    oInfo.m_eRequestType = RequestTypePost;
                else if (sRequestType == "GET" )
                    oInfo.m_eRequestType = RequestTypeGet;
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

bool ServiceHost::ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest )
{
    bool     bHandled = false;
    Service *pService = NULL;

    try
    {
        if (pRequest)
        {
            if (pRequest->m_sBaseUrl != m_sBaseUrl )
                return false;

            VERBOSE( VB_UPNP, QString("ServiceHost::ProcessRequest: %1 : %2")
                                 .arg( pRequest->m_sMethod     )
                                 .arg( pRequest->m_sRawRequest ));

            // --------------------------------------------------------------
            // Check to see if they are requestion the WSDL service Definition
            // --------------------------------------------------------------

            if (( pRequest->m_eType   == RequestTypeGet ) &&
                ( pRequest->m_sMethod == "wsdl"         ))
            {
                Wsdl wsdl( this );

                return wsdl.GetWSDL( pRequest );
            }

            if (( pRequest->m_eType   == RequestTypeGet ) &&
                ( pRequest->m_sMethod == "version"         ))
            {

                int nClassIdx = m_oMetaObject.indexOfClassInfo( "version" );

                if (nClassIdx >=0)
                {
                    QString sVersion = m_oMetaObject.classInfo( nClassIdx ).value();

                    return FormatResponse( pRequest, QVariant( sVersion ));
                }
            }

            // --------------------------------------------------------------

            if (m_Methods.contains( pRequest->m_sMethod ))
            {
                MethodInfo oInfo = m_Methods.value( pRequest->m_sMethod );

                if (( pRequest->m_eType & oInfo.m_eRequestType ) != 0)
                {
                    // ------------------------------------------------------
                    // Create new Instance of the Service Class so
                    // it's guarenteed to be on the same thread
                    // since we are making direct calls into it.
                    // ------------------------------------------------------

                    pService =  qobject_cast< Service* >( m_oMetaObject.newInstance());

                    QVariant vResult = oInfo.Invoke( pService, pRequest->m_mapParams );

                    bHandled = FormatResponse( pRequest, vResult );
                }
            }

            if (!bHandled)
                UPnp::FormatErrorResponse( pRequest, UPnPResult_InvalidAction );
        }
    }
    catch( QString sMsg )
    {
        VERBOSE( VB_IMPORTANT, sMsg );

        UPnp::FormatErrorResponse( pRequest, UPnPResult_ActionFailed, sMsg );
    }
    catch( ... )
    {
        QString sMsg( "ServiceHost::ProcessRequest - Unexpected Exception" );

        VERBOSE( VB_IMPORTANT, sMsg );

        UPnp::FormatErrorResponse( pRequest, UPnPResult_ActionFailed, sMsg );
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

bool ServiceHost::FormatResponse( HTTPRequest *pRequest, QFileInfo *pInfo )
{
    if (pInfo != NULL)
    {
        if (pInfo->isSymLink())
            pRequest->FormatFileResponse( pInfo->symLinkTarget() );
        else
            pRequest->FormatFileResponse( pInfo->absoluteFilePath() );

        delete pInfo;
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

    if ( vValue.canConvert< QFileInfo* >()) 
    {
        const QFileInfo *pFileInfo = vValue.value< QFileInfo* >(); 

        return FormatResponse( pRequest, (QFileInfo*)pFileInfo );
    }

    // ----------------------------------------------------------------------
    // Simple Variant... serialize it.
    // ----------------------------------------------------------------------
    
    Serializer *pSer = pRequest->GetSerializer();

    pSer->Serialize( vValue, "value" );

    pRequest->FormatActionResponse( pSer );

    return true;
}
