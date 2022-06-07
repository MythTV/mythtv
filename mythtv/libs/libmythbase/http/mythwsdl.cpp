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

#include <QFileInfo>

#include "mythwsdl.h"

#include "http/mythmimedatabase.h"
#include "http/mythhttpmetaservice.h"
#include "http/mythhttprequest.h"
#include "http/mythhttpresponse.h"


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////


HTTPResponse MythWSDL::GetWSDL(const HTTPRequest2& Request)
{
    m_typesToInclude.clear();

    if (!Request->m_queries.contains("raw"))
    {
        appendChild( createProcessingInstruction( "xml-stylesheet",
                    R"(type="text/xsl" href="/xslt/service.xslt")" ));
    }

    QDomElement  oNode;

    QString sClassName       = m_pMetaService->m_name;
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
    oNode.setAttribute( "transport", "http://schemas.xmlsoap.org/soap/http" );

    m_oBindings.appendChild( oNode );

    // ----------------------------------------------------------------------
    // Loop for each method in class
    // ----------------------------------------------------------------------

    for (auto & [path, handler] : m_pMetaService->m_slots)
    {
        QString sRequestTypeName  = path;
        QString sResponseTypeName = path + "Response";

        QString sInputMsgName  = QString( "%1_%2_InputMessage"  )
                                    .arg( sClassName,
                                          path );
        QString sOutputMsgName = QString( "%1_%2_OutputMessage" )
                                    .arg( sClassName,
                                          path );

        // ------------------------------------------------------------------
        // Create PortType Operations
        // ------------------------------------------------------------------

        QDomElement oOp = createElement( "operation" );

        oOp.setAttribute( "name", path );

        // ------------------------------------------------------------------
        // Add Operation Description
        // ------------------------------------------------------------------

        QString sDescription;

        if (handler->m_requestTypes & HTTPGet)
            sDescription = "GET ";
        else
            sDescription = "POST ";

        oNode = createElement( "documentation" );
        oNode.appendChild( createTextNode( sDescription ));

        oOp.appendChild( oNode );

        // ------------------------------------------------------------------
        // Create PortType input element
        // ------------------------------------------------------------------

        oNode = createElement( "input" );
        oNode.setAttribute( "wsaw:Action", QString( "%1/%2/%3" )
                                              .arg( sTargetNamespace,
                                                    sClassName,
                                                    path ));
        oNode.setAttribute( "message"    , "tns:" + sInputMsgName );

        oOp.appendChild( oNode );

        // ------------------------------------------------------------------
        // Create PortType output element
        // ------------------------------------------------------------------

        oNode = createElement( "output" );
        oNode.setAttribute( "wsaw:Action", QString( "%1/%2/%3Response" )
                                        .arg( sTargetNamespace,
                                              sClassName,
                                              path ));
        oNode.setAttribute( "message", "tns:" + sOutputMsgName );

        oOp.appendChild( oNode );

        m_oPortType.appendChild( oOp );

        // ------------------------------------------------------------------
        // Create Messages
        // ------------------------------------------------------------------

        QDomElement oMsg = CreateMessage( sInputMsgName, sRequestTypeName );

        m_oRoot.insertAfter( oMsg, m_oLastMsg );
        m_oLastMsg = oMsg;

        // ------------------------------------------------------------------
        // Create Request Type
        // ------------------------------------------------------------------

        m_oTypes.appendChild( CreateMethodType( handler, sRequestTypeName ) );

        // ------------------------------------------------------------------
        // Create Response message
        // ------------------------------------------------------------------

        oMsg = CreateMessage( sOutputMsgName, sResponseTypeName );

        m_oRoot.insertAfter( oMsg, m_oLastMsg );
        m_oLastMsg = oMsg;

        // ------------------------------------------------------------------
        // Create Response Type
        // ------------------------------------------------------------------

        m_oTypes.appendChild( CreateMethodType( handler, sResponseTypeName, true ) );

        // ------------------------------------------------------------------
        // Create Soap Binding Operations
        // ------------------------------------------------------------------
        m_oBindings.appendChild( CreateBindingOperation( path, handler, sClassName ));
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
                           ReadClassInfo(  &m_pMetaService->m_meta,
                                         "Version" );

    sDescription += " - " + ReadClassInfo(  &m_pMetaService->m_meta,
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
                                    Request->m_headers->value("host") + "/" +
                                    m_pMetaService->m_name );
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

        QString sBaseUri = "http://" + Request->m_headers->value("host") +"/" + m_pMetaService->m_name + "/xsd";

        QMap<QString, TypeInfo>::const_iterator it2 = m_typesToInclude.constBegin();
        while( it2 != m_typesToInclude.constEnd())
        {
            QDomElement oIncNode = createElement( "xs:import" );
            QString     sType    = it2.key();
            TypeInfo    info     = it2.value();

            // sType.remove( "DTC::" );
            if (sType.startsWith("V2"))
                sType.remove(0,2);
            sType.remove( "*" );

            QString sValue = QString( "%1?%2=%3" ).arg( sBaseUri,
                                                        info.sAttrName,
                                                        sType);

            if (!info.sContentType.isEmpty())
                sValue += "&name=" + info.sContentType;

            oIncNode.setAttribute( "schemaLocation", sValue );
            oIncNode.setAttribute( "namespace"     , "http://mythtv.org" );

            oImportNode.appendChild( oIncNode );
            ++it2;
        }
    }

    // ----------------------------------------------------------------------
    // Return wsdl doc to caller
    // ----------------------------------------------------------------------

    // Create the XML result
    auto data = MythHTTPData::Create(toByteArray());
    data->m_mimeType = MythMimeDatabase::MimeTypeForName("application/xml");
    data->m_cacheType = HTTPETag | HTTPShortLife;
    return MythHTTPResponse::DataResponse(Request, data);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QDomElement MythWSDL::CreateBindingOperation( const QString& path,
                                          const HTTPMethodPtr& handler,
                                          const QString &sClassName)
{
    // ------------------------------------------------------------------
    // Create PortType Operation
    // ------------------------------------------------------------------

    QDomElement oOp = createElement( "operation" );

    oOp.setAttribute( "name", path );

    QDomElement oNode = createElement( "soap:operation" );
    oNode.setAttribute( "soapAction", QString( "http://mythtv.org/%1/%2" )
                                         .arg( sClassName,
                                               path ));
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

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    const char * typeName = QMetaType::typeName(handler->m_types[0]);
#else
    const char * typeName = QMetaType(handler->m_types[0]).name();
#endif

    if (QString::compare(typeName, "void", Qt::CaseInsensitive ) != 0)
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

QDomElement MythWSDL::CreateMessage( const QString& sMsgName,
                                 const QString& sTypeName )
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

QDomElement MythWSDL::CreateMethodType( const HTTPMethodPtr& handler,
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

        // QString sType = oInfo.m_oMethod.typeName();
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        const char * typeName = QMetaType::typeName(handler->m_types[0]);
#else
        const char * typeName = QMetaType(handler->m_types[0]).name();
#endif

        QString sType(typeName);
        // if (sType.startsWith("V2"))
        //     sType.remove(0,2);
        // sType.remove( "*" );

        sTypeName.remove( "Response" );

        oNode.setAttribute( "minOccurs", 0       );
        oNode.setAttribute( "name"     , sTypeName + "Result" );
        oNode.setAttribute( "nillable" , static_cast<int>(true)    );   //-=>TODO: This may need to be determined by sParamType

        bool bCustomType = IsCustomType( sType );
        sType = MythXSD::ConvertTypeToXSD( sType, bCustomType );
        QString sPrefix = "xs:";

        if (sType.startsWith("V2"))
        {
            sType.remove(0,2);
            sType.remove( "*" );
        }

        if (bCustomType)
        {
            sPrefix = "tns:";

            sType = AddTypeInfo( sType );
        }

        oNode.setAttribute( "type", sPrefix + sType );

        oSeqNode.appendChild( oNode );
    }
    else
    {

        size_t typecount = std::min(handler->m_types.size(), static_cast<size_t>(100));

        // Parameters
        // Iterate over the method's parameters
        size_t count = 1;
        QList<QByteArray> paramNames;
        QList<QByteArray> paramTypes;
        for (count=1; count < typecount; count++)
        {
            auto name  = handler->m_names[count];
            paramNames.append(name.toUtf8());
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            const char * typeName = QMetaType::typeName(handler->m_types[count]);
#else
            const char * typeName = QMetaType(handler->m_types[count]).name();
#endif
            paramTypes.append(QByteArray(typeName));
        }

        for( int nIdx = 0; nIdx < paramNames.length(); nIdx++ )
        {
            QString sName      = paramNames[ nIdx ];
            QString sParamType = paramTypes[ nIdx ];

            bool bCustomType = IsCustomType( sParamType );
            sParamType = MythXSD::ConvertTypeToXSD( sParamType, bCustomType );
            QString sPrefix = "xs:";

            if (bCustomType)
            {
                sPrefix = "tns:";

                sParamType = AddTypeInfo( sParamType );
            }

            QDomElement oNode = createElement( "xs:element" );

            oNode.setAttribute( "minOccurs", 0     );
            oNode.setAttribute( "name"     , sName );
            oNode.setAttribute( "nillable" , static_cast<int>(true)  );   //-=>TODO: This may need to be determined by sParamType
            oNode.setAttribute( "type"     , sPrefix + sParamType );

            oSeqNode.appendChild( oNode );
        }
    }

    return oElementNode;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool MythWSDL::IsCustomType( const QString &sTypeName )
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    int id = QMetaType::type( sTypeName.toUtf8() );
#else
    int id = QMetaType::fromName( sTypeName.toUtf8() ).id();
#endif

    switch( id )
    {
        case QMetaType::QStringList:
        case QMetaType::QVariantList:
        case QMetaType::QVariantMap:
            return true;

        default:
            // for now, treat QFileInfo as a string.  Need to turn into MTOM later.
            if (id == qMetaTypeId<QFileInfo>())
                return false;
            if (sTypeName == "std::chrono::seconds")
                return false;
            break;
    }

    return !(id < QMetaType::User);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString MythWSDL::AddTypeInfo( QString sType )
{
    QString sCustomAttr = "type";

    // if sParamType contains "::", then assume it's a enum
    // (this needs to be looked at again for a better approach)

    if (sType.contains( "::" ))
    {
        sType  = sType.replace( "::", "." );
        sCustomAttr = "enum";
    }

    TypeInfo info = { sCustomAttr, QString() };

    m_typesToInclude.insert( sType, info );

    return sType;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString MythWSDL::ReadClassInfo( const QMetaObject *pMeta, const QString &sKey )
{
    int nIdx = -1;

    if (pMeta)
        nIdx = pMeta->indexOfClassInfo( sKey.toUtf8() );

    if (nIdx >=0)
        return pMeta->classInfo( nIdx ).value();

    return {};
}
