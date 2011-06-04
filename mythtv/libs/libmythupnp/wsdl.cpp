//////////////////////////////////////////////////////////////////////////////
// Program Name: wsdl.cpp
// Created     : Jan. 19, 2010
//
// Purpose     : WSDL XML Generation Class 
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

#include "wsdl.h"

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
    m_typesCreated.clear();

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
    // Add Service Port 
    // ------------------------------------------------------------------

    QDomElement oPort = createElement( "port" );

    oPort.setAttribute( "name"   , QString("BasicHttpBinding_%1"    ).arg( sClassName ));
    oPort.setAttribute( "binding", QString("tns:BasicHttpBinding_%1").arg( sClassName ));

    oNode = createElement( "soap:address" );
    oNode.setAttribute( "location", "http://localhost:6544/" + m_pServiceHost->GetServiceControlURL() );

    oPort.appendChild( oNode );
    m_oService.appendChild( oPort );

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

//        sType.remove( "DTC::" );
        sType.remove( QChar('*') );

        sTypeName.remove( "Response" );

        oNode.setAttribute( "minOccurs", 0       );
        oNode.setAttribute( "name"     , sTypeName + "Result" );
        oNode.setAttribute( "nillable" , true    );   //-=>TODO: This may need to be determined by sParamType

        bool bCustomType = CreateType( NULL, sType );

        oNode.setAttribute( "type"     , "tns:" + ConvertTypeToXSD( sType, bCustomType ));

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
            oNode.setAttribute( "type"     , ConvertTypeToXSD( sParamType ));

            oSeqNode.appendChild( oNode );
        }
    }

    return oElementNode;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Wsdl::CreateType( QObject *pParent, QString &sTypeName )
{
    if ( m_typesCreated.contains( sTypeName ))
        return true;

//    sTypeName.remove( "DTC::" );
    sTypeName.remove( QChar('*') );

    int id = QMetaType::type( sTypeName.toUtf8() );

    switch( id )
    {
        case QMetaType::QStringList:
            return CreateArrayType( pParent, sTypeName, "Value",  "string", true );

        case QMetaType::QVariantMap:
            return CreateMapType( pParent, sTypeName, "Settings", "string", true );

        default:
            // for not, treat QFileInfo as a string.  Need to turn into MTOM later.
            if (id == QMetaType::type( "QFileInfo" ))
            {
                sTypeName = "QString";
                return false;
            }
            break;
    }

    if ((id == -1) || (id < QMetaType::User)) 
        return false;

    // ------------------------------------------------------------------
    // Need to create an instance of the class to access it's metadata.
    // ------------------------------------------------------------------

    QObject *pClass = (QObject *)QMetaType::construct( id );

    if (pClass != NULL)
    {
        const QMetaObject *pMetaObject = pClass->metaObject();

        // ------------------------------------------------------------------
        // Create xsd element structure
        // ------------------------------------------------------------------
    
        QDomElement oTypeNode = createElement( "xs:complexType" );
        QDomElement oSeqNode  = createElement( "xs:sequence"    );

        oTypeNode.setAttribute( "name", ConvertTypeToXSD( sTypeName, true));

//        oElementNode.appendChild( oTypeNode );
        oTypeNode   .appendChild( oSeqNode  );
    
        // ------------------------------------------------------------------
        // Add all properties for this type
        // ------------------------------------------------------------------
    
		int nCount = pMetaObject->propertyCount();

		for (int nIdx=0; nIdx < nCount; ++nIdx ) 
		{
			QMetaProperty metaProperty = pMetaObject->property( nIdx );

            if (metaProperty.isDesignable( pClass ))
            {
                const char *pszPropName = metaProperty.name();
                QString     sPropName( pszPropName );

                if ( sPropName.compare( "objectName" ) == 0)
                    continue;

                QDomElement oNode = createElement( "xs:element" );

                QString sType = metaProperty.typeName();

                // if this is a child object, sType will be QObject*
                // which we can't use, so we need to read the
                // properties value, and read it's metaObject data

                if (sType == "QObject*")
                {
                    QVariant val = metaProperty.read( pClass );
                    const QObject *pObject = val.value< QObject* >(); 

                    sType = pObject->metaObject()->className();

//                    sType.remove( "DTC::" );
                }

                oNode.setAttribute( "minOccurs", 0       );
                oNode.setAttribute( "name"     , metaProperty.name() );
                oNode.setAttribute( "nillable" , true    );   //-=>TODO: This may need to be determined by sParamType
    
                bool bCustomType = CreateType( pClass, sType );

                oNode.setAttribute( "type"     , ((bCustomType) ? "tns:" : "") + ConvertTypeToXSD( sType, bCustomType ));

                oSeqNode.appendChild( oNode );
            }
		}

        QDomElement oElementNode = createElement( "xs:element" );

        oElementNode.setAttribute( "name"    , ConvertTypeToXSD( sTypeName, true));
        oElementNode.setAttribute( "nillable", "true" );
        oElementNode.setAttribute( "type"    , "tns:" + ConvertTypeToXSD( sTypeName, true));

        m_oTypes.appendChild( oTypeNode    );
        m_oTypes.appendChild( oElementNode );

        QMetaType::destroy( id, pClass );
     }

    m_typesCreated.insert( sTypeName, true );

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
// <complexType name="ArrayOf<contained_typename>" >
//   <sequence>
//     <element name="<contained_typename>" type="<ConvertTypeToXsd(contained_typename)" minOccurs="0" maxOccurs="unbounded"/>
//   </sequence>
// </complexType>
// <element name="<typename>" nillable="true" type="tns:ArrayOf<contained_typename>" />
//
/*
<xs:complexType name="ArrayOfstring">
		<xs:sequence>
			<xs:element name="String" type="xs:string" maxOccurs="unbounded"/>
		</xs:sequence>
	</xs:complexType>
	<xs:element name="StringList" type="ArrayOfstring"/>
    */
/////////////////////////////////////////////////////////////////////////////

bool Wsdl::CreateArrayType( QObject      *pParent,
                            QString      &sTypeName, 
                            QString       sElementName,
                            QString       sElementType,
                            bool          bCustomType )
{
    QString sOrigTypeName( sTypeName );

    sTypeName = "ArrayOf" + sElementType;

    if ( m_typesCreated.contains( sTypeName ))
        return bCustomType;

    QDomElement oTypeNode = createElement( "xs:complexType" );
    QDomElement oSeqNode  = createElement( "xs:sequence"    );

    oTypeNode.setAttribute( "name", sTypeName );

    oTypeNode.appendChild( oSeqNode  );

    QDomElement oNode = createElement( "xs:element" );

    oNode.setAttribute( "name"     , sElementName );
    oNode.setAttribute( "type"     , ConvertTypeToXSD( sElementType, false ));

    oNode.setAttribute( "nillable" , true        );   //-=>TODO: This may need to be determined by sParamType
    oNode.setAttribute( "minOccurs", 0           );
    oNode.setAttribute( "maxOccurs", "unbounded" );

    oSeqNode.appendChild( oNode );

    QDomElement oElementNode = createElement( "xs:element" );

    oElementNode.setAttribute( "name"    , sTypeName );
    oElementNode.setAttribute( "nillable", "true" );
    oElementNode.setAttribute( "type"    , "tns:" + sTypeName );

    m_oTypes.appendChild( oTypeNode    );
    m_oTypes.appendChild( oElementNode );

    m_typesCreated.insert( sTypeName, true );

    return bCustomType;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Wsdl::CreateMapType( QObject      *pParent,
                          QString      &sTypeName, 
                          QString       sElementName,
                          QString       sElementType,
                          bool          bCustomType )
{
/*
    QString sOrigTypeName( sTypeName );

    sTypeName = "MapOf" + sElementType;

    if ( m_typesCreated.contains( sTypeName ))
        return bCustomType;

    QString sMapType = ReadClassInfo( pParent, sElementName + "_type" );

    CreateType( NULL, sMapType );

    QDomElement oTypeNode = createElement( "xs:complexType" );
    QDomElement oSeqNode  = createElement( "xs:all"    );

    oTypeNode.setAttribute( "name", sTypeName );

    oTypeNode.appendChild( oSeqNode  );

    QDomElement oNode = createElement( "xs:element" );

    oNode.setAttribute( "name"     , sElementName );
    oNode.setAttribute( "type"     , ConvertTypeToXSD( sElementType, false ));

    oNode.setAttribute( "nillable" , true        );   //-=>TODO: This may need to be determined by sParamType
    oNode.setAttribute( "minOccurs", 0           );
    oNode.setAttribute( "maxOccurs", "unbounded" );

    oSeqNode.appendChild( oNode );

    QDomElement oElementNode = createElement( "xs:element" );

    oElementNode.setAttribute( "name"    , sTypeName );
    oElementNode.setAttribute( "nillable", "true" );
    oElementNode.setAttribute( "type"    , "tns:" + sTypeName );

    m_oTypes.appendChild( oTypeNode    );
    m_oTypes.appendChild( oElementNode );

    m_typesCreated.insert( sTypeName, true );
*/
    return bCustomType;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString Wsdl::ConvertTypeToXSD( const QString &sType, bool bCustomType )
{
    if (bCustomType || sType.startsWith( "DTC::"))
    {
        QString sTypeName( sType );

        sTypeName.remove( "DTC::"    );
        sTypeName.remove( QChar('*') );

        return sTypeName;
    }

    if (sType == "QDateTime")
        return "xs:dateTime";

    if (sType == "bool")
        return "xs:boolean";

    if (sType.at(0) == 'Q')
        return "xs:" + sType.mid( 1 ).toLower();

    return "xs:" + sType.toLower();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString Wsdl::ReadClassInfo( QObject *pObject, QString sKey )
{
    const QMetaObject *pMeta = pObject->metaObject();

    int nIdx = pMeta->indexOfClassInfo( sKey.toUtf8() );

    if (nIdx >=0)
        return pMeta->classInfo( nIdx ).value();

    return QString();
}
