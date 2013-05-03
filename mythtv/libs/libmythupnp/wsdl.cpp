//////////////////////////////////////////////////////////////////////////////
// Program Name: wsdl.cpp
// Created     : Jan. 19, 2010
//
// Purpose     : WSDL XML Generation Class 
//                                                                            
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#include "wsdl.h"
#include "xsd.h"

#include "servicehost.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Wsdl::Wsdl( ServiceHost *pServiceHost ) : m_pServiceHost( pServiceHost )
{
    
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Wsdl::GetWSDL( HTTPRequest *pRequest )
{
    m_typesToInclude.clear();

    if (!pRequest->m_mapParams.contains( "raw" ))
    {
        appendChild( createProcessingInstruction( "xml-stylesheet",
                    "type=\"text/xsl\" href=\"/xslt/service.xslt\"" ));
    }

    QDomElement  oNode;

    QString sClassName       = m_pServiceHost->GetServiceMetaObject().className();
    QString sTargetNamespace = "http://mythtv.org";
    
    m_oRoot = createElementNS( "http://schemas.xmlsoap.org/wsdl/", "definitions");

    m_oRoot.setAttribute( "targetNamespace", sTargetNamespace );

    m_oRoot.setAttribute( "xmlns:soap", "http://schemas.xmlsoap.org/wsdl/soap/" );
    m_oRoot.setAttribute( "xmlns:xs"  , "http://www.w3.org/2001/XMLSchema"      );
    m_oRoot.setAttribute( "xmlns:soap", "http://schemas.xmlsoap.org/wsdl/soap/" );
    m_oRoot.setAttribute( "xmlns:tns" , sTargetNamespace );
    m_oRoot.setAttribute( "xmlns:wsaw", "http://www.w3.org/2006/05/addressing/wsdl" );

    m_oRoot.setAttribute( "name", QString( "%1Services" ).arg( sClassName ) );

    m_oTypes    = createElement( "types"    );
    m_oLastMsg  = m_oTypes;
    m_oPortType = createElement( "portType" );
    m_oBindings = createElement( "binding"  );
    m_oService  = createElement( "service"  );

            appendChild( m_oRoot      );
    m_oRoot.appendChild( m_oTypes    );
    m_oRoot.appendChild( m_oPortType );
    m_oRoot.appendChild( m_oBindings );
    m_oRoot.appendChild( m_oService  );

    m_oPortType.setAttribute( "name", sClassName );

    // ----------------------------------------------------------------------

    QDomElement oImportNode = createElement( "xs:schema" );
    oImportNode.setAttribute( "targetNamespace"   , "http://MythTV.org/Imports" );

    m_oTypes.appendChild( oImportNode );

    // ----------------------------------------------------------------------

    oNode = createElement( "xs:schema" );
    oNode.setAttribute( "targetNamespace"   , sTargetNamespace );
    oNode.setAttribute( "elementFormDefault", "qualified"      );

    m_oTypes.appendChild( oNode );
    m_oTypes = oNode;

    // ----------------------------------------------------------------------
    // Add Bindings...
    // ----------------------------------------------------------------------

    m_oBindings.setAttribute( "name", QString("BasicHttpBinding_%1").arg( sClassName ));
    m_oBindings.setAttribute( "type", QString( "tns:%1"            ).arg( sClassName ));

    oNode = createElement( "soap:binding" );
    //oNode.setAttribute( "style"    , "document" );
    oNode.setAttribute( "transport", "http://schemas.xmlsoap.org/soap/http" );

    m_oBindings.appendChild( oNode );

    // ----------------------------------------------------------------------
    // Loop for each method in class
    // ----------------------------------------------------------------------

    QMapIterator< QString, MethodInfo > it( m_pServiceHost->GetMethods() );

    while( it.hasNext()) 
    {
        it.next();

        MethodInfo oInfo = it.value();

        QString sRequestTypeName  = oInfo.m_sName;
        QString sResponseTypeName = oInfo.m_sName + "Response";

        QString sInputMsgName  = QString( "%1_%2_InputMessage"  )
                                    .arg( sClassName )
                                    .arg( oInfo.m_sName );
        QString sOutputMsgName = QString( "%1_%2_OutputMessage" )
                                    .arg( sClassName )
                                    .arg( oInfo.m_sName );

        // ------------------------------------------------------------------
        // Create PortType Operations
        // ------------------------------------------------------------------

        QDomElement oOp = createElement( "operation" );
    
        oOp.setAttribute( "name", oInfo.m_sName );

        // ------------------------------------------------------------------
        // Add Operation Description
        // ------------------------------------------------------------------

        QString sDescription;

        if ( oInfo.m_eRequestType == RequestTypePost ) 
            sDescription = "POST ";
        else
            sDescription = "GET ";

        sDescription += ReadClassInfo( &m_pServiceHost->GetServiceMetaObject(),
                                       "description" );

        oNode = createElement( "documentation" );
        oNode.appendChild( createTextNode( sDescription ));

        oOp.appendChild( oNode );
    
        // ------------------------------------------------------------------
        // Create PortType input element 
        // ------------------------------------------------------------------
    
        oNode = createElement( "input" );
        oNode.setAttribute( "wsaw:Action", QString( "%1/%2/%3" )
                                              .arg( sTargetNamespace )
                                              .arg( sClassName )
                                              .arg( oInfo.m_sName ));
        oNode.setAttribute( "message"    , "tns:" + sInputMsgName );
    
        oOp.appendChild( oNode );
    
        // ------------------------------------------------------------------
        // Create PortType output element 
        // ------------------------------------------------------------------
    
        oNode = createElement( "output" );
        oNode.setAttribute( "wsaw:Action", QString( "%1/%2/%3Response" )
                                        .arg( sTargetNamespace )
                                        .arg( sClassName )
                                        .arg( oInfo.m_sName ));
        oNode.setAttribute( "message", "tns:" + sOutputMsgName );
    
        oOp.appendChild( oNode );

        m_oPortType.appendChild( oOp );

        // ------------------------------------------------------------------
        // Create Messages
        // ------------------------------------------------------------------

        QDomElement oMsg = CreateMessage( oInfo, sInputMsgName, sRequestTypeName );

        m_oRoot.insertAfter( oMsg, m_oLastMsg );
        m_oLastMsg = oMsg;

        // ------------------------------------------------------------------
        // Create Request Type
        // ------------------------------------------------------------------

        m_oTypes.appendChild( CreateMethodType( oInfo, sRequestTypeName ) );

        // ------------------------------------------------------------------
        // Create Response message 
        // ------------------------------------------------------------------

        oMsg = CreateMessage( oInfo, sOutputMsgName, sResponseTypeName );

        m_oRoot.insertAfter( oMsg, m_oLastMsg );
        m_oLastMsg = oMsg;

        // ------------------------------------------------------------------
        // Create Response Type
        // ------------------------------------------------------------------

        m_oTypes.appendChild( CreateMethodType( oInfo, sResponseTypeName, true ) );
        
        // ------------------------------------------------------------------
        // Create Soap Binding Operations
        // ------------------------------------------------------------------

        m_oBindings.appendChild( CreateBindingOperation( oInfo, sClassName ));
    }

    // ----------------------------------------------------------------------
    // Add Service Details
    // ----------------------------------------------------------------------

    QString sServiceName = QString( "%1Services" ).arg( sClassName );

    m_oService.setAttribute( "name", sServiceName );

    // ------------------------------------------------------------------
    // Add Service Description
    // ------------------------------------------------------------------

    QString sDescription = "Interface Version " +
                           ReadClassInfo( &m_pServiceHost->GetServiceMetaObject(), 
                                         "version" );

    sDescription += " - " + ReadClassInfo( &m_pServiceHost->GetServiceMetaObject(), 
                                         "description" );

    oNode = createElement( "documentation" );
    oNode.appendChild( createTextNode( sDescription ));

    m_oService.appendChild( oNode );

    // ------------------------------------------------------------------
    // Add Service Port 
    // ------------------------------------------------------------------

    QDomElement oPort = createElement( "port" );

    oPort.setAttribute( "name"   , QString("BasicHttpBinding_%1"    ).arg( sClassName ));
    oPort.setAttribute( "binding", QString("tns:BasicHttpBinding_%1").arg( sClassName ));

    oNode = createElement( "soap:address" );
    oNode.setAttribute( "location", "http://" + 
                                    pRequest->m_mapHeaders[ "host" ] + "/" +
                                    m_pServiceHost->GetServiceControlURL() );

    oPort.appendChild( oNode );
    m_oService.appendChild( oPort );

    // ----------------------------------------------------------------------
    //
    // ----------------------------------------------------------------------

    if (m_typesToInclude.count() > 0)
    {
        // ------------------------------------------------------------------
        // Create all needed includes
        //
        //	<xs:import schemaLocation="<path to dependant schema" namespace="http://mythtv.org"/>
        // ------------------------------------------------------------------

        QString sBaseUri = "http://" + pRequest->m_mapHeaders[ "host" ] + pRequest->m_sBaseUrl + "/xsd?type=";

        QMap<QString, bool>::const_iterator it = m_typesToInclude.constBegin();
        while( it != m_typesToInclude.constEnd())
        {
            QDomElement oIncNode = createElement( "xs:import" );
            QString     sType    = it.key();

            sType.remove( "DTC::" );

            oIncNode.setAttribute( "schemaLocation", sBaseUri + sType );
            oIncNode.setAttribute( "namespace", "http://mythtv.org" );
    
            oImportNode.appendChild( oIncNode );
            ++it;
        }
    }


    // ----------------------------------------------------------------------
    // Return wsdl doc to caller
    // ----------------------------------------------------------------------

    QTextStream os( &(pRequest->m_response) );

    pRequest->m_eResponseType   = ResponseTypeXML;

    save( os, 0 );
    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QDomElement Wsdl::CreateBindingOperation( MethodInfo    &oInfo,
                                          const QString &sClassName)
{
    // ------------------------------------------------------------------
    // Create PortType Operation
    // ------------------------------------------------------------------

    QDomElement oOp = createElement( "operation" );

    oOp.setAttribute( "name", oInfo.m_sName );

    QDomElement oNode = createElement( "soap:operation" );
    oNode.setAttribute( "soapAction", QString( "http://mythtv.org/%1/%2" )
                                         .arg( sClassName )
                                         .arg( oInfo.m_sName ));
    oNode.setAttribute( "style"     , "document" );

    oOp.appendChild( oNode );

    // ------------------------------------------------------------------
    // Create PortType input element 
    // ------------------------------------------------------------------

    QDomElement oDirection = createElement( "input" );
    oNode = createElement( "soap:body" );
    oNode.setAttribute( "use", "literal" );

    oDirection.appendChild( oNode );
    oOp.appendChild( oDirection );

    if (QString::compare( oInfo.m_oMethod.typeName(), "void", Qt::CaseInsensitive ) != 0)
    {
        // Create output element 

        oDirection = createElement( "output" );

        oNode = createElement( "soap:body" );
        oNode.setAttribute( "use", "literal" );

        oDirection.appendChild( oNode );
        oOp.appendChild( oDirection );

    }

    return oOp;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QDomElement Wsdl::CreateMessage( MethodInfo   &oInfo,
                                 QString       sMsgName, 
                                 QString       sTypeName )
{
    QDomElement oMsg = createElement( "message" );

    oMsg.setAttribute( "name", sMsgName );

    QDomElement oNode = createElement( "part" );

    oNode.setAttribute( "name"   , "parameters" );
    oNode.setAttribute( "element", "tns:" + sTypeName    );

    oMsg.appendChild( oNode );

    return oMsg;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QDomElement Wsdl::CreateMethodType( MethodInfo   &oInfo,
                                    QString       sTypeName,
                                    bool          bReturnType /* = false */)
{
    QDomElement oElementNode = createElement( "xs:element" );

    oElementNode.setAttribute( "name", sTypeName );

    QDomElement oTypeNode = createElement( "xs:complexType" );
    QDomElement oSeqNode  = createElement( "xs:sequence"    );
    
    oElementNode.appendChild( oTypeNode );
    oTypeNode   .appendChild( oSeqNode  );

    // ------------------------------------------------------------------
    // Create message for parameters
    // ------------------------------------------------------------------

    if (bReturnType)
    {
        QDomElement oNode = createElement( "xs:element" );

        QString sType = oInfo.m_oMethod.typeName();

        sType.remove( QChar('*') );

        sTypeName.remove( "Response" );

        oNode.setAttribute( "minOccurs", 0       );
        oNode.setAttribute( "name"     , sTypeName + "Result" );
        oNode.setAttribute( "nillable" , true    );   //-=>TODO: This may need to be determined by sParamType

        bool bCustomType = IsCustomType( sType );

        sType = Xsd::ConvertTypeToXSD( sType, bCustomType );

        QString sPrefix = "xs:";

        if (bCustomType)
        {
            sPrefix = "tns:";


            m_typesToInclude.insert( sType, true );
        }

        oNode.setAttribute( "type", sPrefix + sType );

        oSeqNode.appendChild( oNode );
    }
    else
    {
        QList<QByteArray> paramNames = oInfo.m_oMethod.parameterNames();
        QList<QByteArray> paramTypes = oInfo.m_oMethod.parameterTypes();

        for( int nIdx = 0; nIdx < paramNames.length(); nIdx++ )
        {
            QString sName      = paramNames[ nIdx ];
            QString sParamType = paramTypes[ nIdx ];

            QDomElement oNode = createElement( "xs:element" );

            oNode.setAttribute( "minOccurs", 0     );
            oNode.setAttribute( "name"     , sName );
            oNode.setAttribute( "nillable" , true  );   //-=>TODO: This may need to be determined by sParamType
            oNode.setAttribute( "type"     , "xs:" + Xsd::ConvertTypeToXSD( sParamType ));

            oSeqNode.appendChild( oNode );
        }
    }

    return oElementNode;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Wsdl::IsCustomType( QString &sTypeName )
{
    sTypeName.remove( QChar('*') );

    int id = QMetaType::type( sTypeName.toUtf8() );

    switch( id )
    {
        case QMetaType::QStringList:
        case QMetaType::QVariantList:
        case QMetaType::QVariantMap:
            return true;

        default:
            // for now, treat QFileInfo as a string.  Need to turn into MTOM later.
            if (id == QMetaType::type( "QFileInfo" ))
                return false;
            break;
    }

    if ((id == -1) || (id < QMetaType::User)) 
        return false;

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString Wsdl::ReadClassInfo( const QMetaObject *pMeta, const QString &sKey )
{
    int nIdx = pMeta->indexOfClassInfo( sKey.toUtf8() );

    if (nIdx >=0)
        return pMeta->classInfo( nIdx ).value();

    return QString();
}
