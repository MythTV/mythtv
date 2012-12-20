//////////////////////////////////////////////////////////////////////////////
// Program Name: xsd.cpp
// Created     : Feb. 20, 2012
//
// Purpose     : XSD Generation Class 
//                                                                            
// Copyright (c) 2012 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#include "xsd.h"

#include "servicehost.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Xsd::GetXSD( HTTPRequest *pRequest, QString sTypeName )
{
    bool    bIsArray     = false;
    bool    bIsMap       = false;

    if (sTypeName.isEmpty())
        return false;

    // ------------------------------------------------------------------
    // Is this a special type name?
    // ------------------------------------------------------------------
    
    if (sTypeName.startsWith( "ArrayOf" ))
    {
        bIsArray = true;
        sTypeName = sTypeName.mid( 7 );
    }

    if (sTypeName.startsWith( "MapOfString" ))
    {
        bIsMap       = true;
        sTypeName    = sTypeName.mid( 11 );
    }

    // ------------------------------------------------------------------
    // Check to see if one of the Qt Types we need to handle special
    // ------------------------------------------------------------------

    int id = QMetaType::type( sTypeName.toUtf8() );

    // ------------------------------------------------------------------
    // if a DataContract type, we need to prefix name with DTC::
    // ------------------------------------------------------------------

    if (id == 0)
    {
        QString sFQN = "DTC::" + sTypeName;

        id = QMetaType::type( sFQN.toUtf8() );
    }

    // ------------------------------------------------------------------
    //
    // ------------------------------------------------------------------

    if (!(bIsArray || bIsMap) && ((id == -1) || (id < QMetaType::User)))
        return false;

    // ------------------------------------------------------------------
    // Need to create an instance of the class to access it's metadata.
    // ------------------------------------------------------------------

    bool     bHandled = false;

    // --------------------------------------------------------------
    // Render XSD
    // --------------------------------------------------------------

    if (!pRequest->m_mapParams.contains( "raw" ))
    {
        appendChild( createProcessingInstruction( "xml-stylesheet",
                        "type=\"text/xsl\" href=\"/xslt/class.xslt\"" ));
    }

    if (bIsArray)
    {
        bHandled = RenderArrayXSD( pRequest, sTypeName, id >= QMetaType::User );
    }
    else if (bIsMap)
    {
        bHandled = RenderMapXSD( pRequest, 
                                 sTypeName, 
                                 id >= QMetaType::User );
    }
    else
    {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
        QObject *pClass = (QObject *)QMetaType::construct( id );
#else
        QObject *pClass = (QObject *)QMetaType::create( id );
#endif

        if (pClass != NULL)
            bHandled = RenderXSD( pRequest, pClass );

        QMetaType::destroy( id, pClass );
    }

    return bHandled;

}

/////////////////////////////////////////////////////////////////////////////
//
//<?xml version="1.0" encoding="UTF-8"?>
//<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" 
//           xmlns:tns="http://mythtv.org" 
//           targetNamespace="http://mythtv.org" 
//           elementFormDefault="qualified" 
//           attributeFormDefault="unqualified">
//	<xs:include schemaLocation="<path to dependant schema"/>
//	
//	<xs:complexType name="<className>">
//		<xs:annotation>
//			<xs:documentation>Comment describing your root element</xs:documentation>
//		</xs:annotation>
//		<xs:sequence>
//			<xs:element minOccurs="0" name="<propName>" type="<propType>"/>
//			<xs:element minOccurs="0" name="<childName>" nillable="true" type="tns:<ChildType>"/>
//		</xs:sequence>
//	</xs:complexType>
//	<xs:element name="<className>" nillable="true" type="tns:<className>"/>
//
//</xs:schema>
/////////////////////////////////////////////////////////////////////////////

bool Xsd::RenderXSD( HTTPRequest *pRequest, QObject *pClass )
{
    const QMetaObject *pMetaObject = pClass->metaObject();

    QString     sClassName = ConvertTypeToXSD( pMetaObject->className(), true);
    QDomElement oRoot      = CreateSchemaRoot();

    QMap<QString, QString> typesToInclude;

    // ------------------------------------------------------------------
    // Create xs:complexType structure
    // ------------------------------------------------------------------
    
    QDomElement oTypeNode = createElement( "xs:complexType" );
    QDomElement oSeqNode  = createElement( "xs:sequence"    );

    oTypeNode.setAttribute( "name", sClassName );

    oTypeNode.appendChild( oSeqNode  );
    
    // -=>TODO: Add an xs:annotation node with class descriptions

    // ------------------------------------------------------------------
    // Add all properties for this type
    //
    //  <xs:element minOccurs="0" name="<propName>" type="<propType>"/>
    //	<xs:element minOccurs="0" name="<childName>" nillable="true" 
    //              type="tns:<ChildType>"/>
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

            // ----------------------------------------------------------
            // Create xs:element for this property
            // ----------------------------------------------------------

            QDomElement oNode        = createElement( "xs:element" );
            QString     sType        = metaProperty.typeName();
            bool        bCustomType  = false;
            QString     sContentName = QString();
            QString     sContentType = QString();

            // if this is a child object, sType will be QObject*
            // which we can't use, so we need to read the
            // properties value, and read it's metaObject data

            if (sType == "QObject*")
            {
                QVariant val = metaProperty.read( pClass );
                const QObject *pObject = val.value< QObject* >(); 

                sType = pObject->metaObject()->className();
                bCustomType = true;
            }
            else if ((sType == "QVariantList" ) || (sType == "QVariantMap"))
            {
                sContentType = ReadPropertyMetadata( pClass, 
                                                     sPropName, 
                                                     "type" );

                if (sContentType.at(0) == 'Q')
                    sContentType = sContentType.mid( 1 );

                sContentType.remove( "DTC::"    );
                sContentType.remove( QChar('*') );

                if (sType == "QVariantMap")
                {
                    sContentName = ReadPropertyMetadata( pClass, 
                                                         sPropName, 
                                                         "name" );

                    if (sContentName.isEmpty())
                        sContentName = sContentType;

                    sType = "MapOfString" + sContentName;
                }
                else
                    sType = "ArrayOf" + sContentType;

                bCustomType = true;

            }
            else if (sType == "QStringList") 
            { 
                sType = "ArrayOfString"; 
                bCustomType = true; 
            }

            QString sNewPropName( metaProperty.name() );

            if (IsNillable( sType ))
                oNode.setAttribute( "nillable" , true );   

            if (bCustomType)
                typesToInclude.insert( sType, sContentType );
    
            oNode.setAttribute( "type"     , ((bCustomType) ? "tns:" : "xs:") +
                                       ConvertTypeToXSD( sType, bCustomType ));

            oNode.setAttribute( "name"     , sNewPropName );
            oNode.setAttribute( "minOccurs", 0            );

            oSeqNode.appendChild( oNode );
        }
    }

    // ------------------------------------------------------------------
    // Create element for class
    //
    //	   <xs:element name="<className>" nillable="true" type="tns:<className>"/>
    // ------------------------------------------------------------------

    QDomElement oElementNode = createElement( "xs:element" );

    oElementNode.setAttribute( "type"    , "tns:" + sClassName );
    oElementNode.setAttribute( "nillable", "true" );
    oElementNode.setAttribute( "name"    , sClassName );

    // ----------------------------------------------------------------------
    // Build xml tree...
    // ----------------------------------------------------------------------

    appendChild( oRoot );

    if (typesToInclude.count() > 0)
    {
        // ------------------------------------------------------------------
        // Create all needed includes
        //
        //	<xs:include schemaLocation="<path to dependant schema"/>
        // ------------------------------------------------------------------

        QString sBaseUri = "http://" + pRequest->m_mapHeaders[ "host" ] + 
                                       pRequest->m_sResourceUrl + "?type=";

        QMap<QString, QString>::const_iterator it = typesToInclude.constBegin();
        while( it != typesToInclude.constEnd())
        {
            QDomElement oIncNode = createElement( "xs:include" );
            QString     sType    = it.key();

            sType.remove( "DTC::" );

            if (it.value().isEmpty())
                oIncNode.setAttribute( "schemaLocation", sBaseUri + sType );
            else
                oIncNode.setAttribute( "schemaLocation", sBaseUri + sType + "&name=" + it.value() );
    
            oRoot.appendChild( oIncNode );
            ++it;
        }
    }

    oRoot.appendChild( oTypeNode    );
    oRoot.appendChild( oElementNode );

    // ----------------------------------------------------------------------
    // Return xsd doc to caller
    // ----------------------------------------------------------------------

    QTextStream os( &(pRequest->m_response) );

    pRequest->m_eResponseType   = ResponseTypeXML;

    save( os, 0 );

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
// ArrayOf<Type>
// 
//<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:tns="http://mythtv.org" targetNamespace="http://mythtv.org" elementFormDefault="qualified" attributeFormDefault="unqualified">
//	<xs:include schemaLocation="<type>.xsd"/>
//	
//	<xs:complexType name="ArrayOf<Type>">
//		<xs:annotation>
//			<xs:documentation>Comment describing your root element</xs:documentation>
//		</xs:annotation>
//		<xs:sequence>
//			<xs:element minOccurs="0" maxOccurs="unbounded" name="<type>" nillable="true" type="tns:<Type>"/>
//		</xs:sequence>
//	</xs:complexType>
//	<xs:element name="ArrayOf<Type>" nillable="true" type="tns:ArrayOf<Type>"/>
//
//</xs:schema>
//
/////////////////////////////////////////////////////////////////////////////

bool Xsd::RenderArrayXSD( HTTPRequest   *pRequest, 
                          const QString &sClassName, 
                          bool           bCustomType )
{
    QString     sArrayName = "ArrayOf" + sClassName;
    QString     sType;

    if (bCustomType)
        sType = "tns:" + sClassName;
    else
        sType = "xs:" + ConvertTypeToXSD( sClassName, false );

    QDomElement oRoot      = CreateSchemaRoot();

    // ------------------------------------------------------------------
    // Create xs:complexType structure
    // ------------------------------------------------------------------
    
    QDomElement oTypeNode = createElement( "xs:complexType" );
    QDomElement oSeqNode  = createElement( "xs:sequence"    );

    // -=>TODO: Add an xs:annotation node with class descriptions

    oTypeNode.setAttribute( "name", sArrayName );
    oTypeNode.appendChild( oSeqNode  );

    QDomElement oNode = createElement( "xs:element" );

    oNode.setAttribute( "type"      , sType );
    oNode.setAttribute( "nillable"  , "true" );
    oNode.setAttribute( "name"      , sClassName );
    oNode.setAttribute( "maxOccurs" , "unbounded" );
    oNode.setAttribute( "minOccurs" , "0"         );

    oSeqNode.appendChild( oNode  );
    
    // ----------------------------------------------------------------------
    //
    // ----------------------------------------------------------------------
    
    QDomElement oElementNode = createElement( "xs:element" );

    oElementNode.setAttribute( "type"    , "tns:" + sArrayName );
    oElementNode.setAttribute( "nillable", "true" );
    oElementNode.setAttribute( "name"    , sArrayName );

    // ----------------------------------------------------------------------
    // Build xml tree...
    // ----------------------------------------------------------------------

    appendChild( oRoot );

    if (bCustomType)
    {
        QDomElement oIncNode = createElement( "xs:include" );

        QString sBaseUri = "http://" + pRequest->m_mapHeaders[ "host" ] + 
                                       pRequest->m_sResourceUrl + "?type=";

        oIncNode.setAttribute( "schemaLocation", sBaseUri + sClassName );

        oRoot.appendChild( oIncNode );
    }
    
    oRoot.appendChild( oTypeNode    );
    oRoot.appendChild( oElementNode );

    // ----------------------------------------------------------------------
    // Return xsd doc to caller
    // ----------------------------------------------------------------------

    QTextStream os( &(pRequest->m_response) );

    pRequest->m_eResponseType   = ResponseTypeXML;

    save( os, 0 );

    return true;

}

/////////////////////////////////////////////////////////////////////////////
//
// MapOfString<Type>
// 
//<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:tns="http://mythtv.org" targetNamespace="http://mythtv.org" elementFormDefault="qualified" attributeFormDefault="unqualified">
//	<xs:include schemaLocation="<type>.xsd"/>
//	
//<xs:complexType name="MapOfString<Type>">
//  <xs:annotation>
//    <xs:appinfo>
//      <IsDictionary xmlns="http://schemas.microsoft.com/2003/10/Serialization/">true</IsDictionary> 
//    </xs:appinfo>
//  </xs:annotation>
//  <xs:sequence>
//    <xs:element minOccurs="0" maxOccurs="unbounded" name="KeyValueOfString<Type>">
//      <xs:complexType>
//        <xs:sequence>
//           <xs:element name="Key" nillable="true" type="xs:string" /> 
//           <xs:element name="Value" nillable="true" type="<Type>"  /> 
//        </xs:sequence>
//      </xs:complexType>
//    </xs:element>
//  </xs:sequence>
//</xs:complexType>
//<xs:element name="MapOfString<Type>" nillable="true" type="tns:MapOfString<Type>" /> 
//
//</xs:schema>
//
/////////////////////////////////////////////////////////////////////////////

bool Xsd::RenderMapXSD( HTTPRequest   *pRequest, 
                        const QString &sClassName, 
                        bool           bCustomType )
{
    QString     sArrayName   = "MapOfString" + sClassName;
    QString     sMapItemName = pRequest->m_mapParams[ "name" ];
    QString     sType;

    if (sMapItemName.isEmpty())
        sMapItemName = sClassName;

    if (bCustomType)
        sType = "tns:" + sMapItemName;
    else
        sType = "xs:" + ConvertTypeToXSD( sMapItemName, false );

    QDomElement oRoot      = CreateSchemaRoot();

    // ------------------------------------------------------------------
    // Create xs:complexType structure
    // ------------------------------------------------------------------
    
    QDomElement oTypeNode = createElement( "xs:complexType" );
    QDomElement oSeqNode  = createElement( "xs:sequence"    );
    QDomElement oAnno     = createElement( "xs:annotation"  );
    QDomElement oAppInfo  = createElement( "xs:appinfo"     );

    QDomElement oNode = createElement( "IsDictionary" );
    oNode.setAttribute( "xmlns", "http://schemas.microsoft.com/2003/10/Serialization/" );
    oNode.appendChild( createTextNode( "true" ));

    oTypeNode.appendChild( oAnno    );
    oAnno    .appendChild( oAppInfo );
    oAppInfo .appendChild( oNode    );

    // -=>TODO: Add an xs:annotation node with class descriptions

    // ----------------------------------------------------------------------
    //  <xs:sequence>
    // ----------------------------------------------------------------------

    oTypeNode.setAttribute( "name", sArrayName );
    oTypeNode.appendChild( oSeqNode  );

    // ----------------------------------------------------------------------
    //    <xs:element minOccurs="0" maxOccurs="unbounded" name="KeyValueOfString<Type>">
    // ----------------------------------------------------------------------

    QDomElement oInnerNode = createElement( "xs:element" );

    oInnerNode.setAttribute( "name"      , sClassName  );
    oInnerNode.setAttribute( "maxOccurs" , "unbounded" );
    oInnerNode.setAttribute( "minOccurs" , "0"         );

    oSeqNode.appendChild( oInnerNode );

    // ----------------------------------------------------------------------
    //      <xs:complexType>
    // ----------------------------------------------------------------------
    oNode = createElement( "xs:complexType" );

    oInnerNode.appendChild( oNode );

    // ----------------------------------------------------------------------
    //        <xs:sequence>
    // ----------------------------------------------------------------------

    QDomElement oInnerSeq = createElement( "xs:sequence" );

    oNode.appendChild( oInnerSeq );

    // ----------------------------------------------------------------------
    //           <xs:element name="Key" nillable="true" type="xs:string" /> 
    // ----------------------------------------------------------------------

    oNode = createElement( "xs:element" );

    oNode.setAttribute( "type"      , "xs:string" );
    oNode.setAttribute( "nillable"  , "true" );
    oNode.setAttribute( "name"      , "Key" );

    oInnerSeq.appendChild( oNode  );

    // ----------------------------------------------------------------------
    //           <xs:element name="Value" nillable="true" type="<Type>"  /> 
    // ----------------------------------------------------------------------

    oNode = createElement( "xs:element" );

    oNode.setAttribute( "type"      , sType );
    oNode.setAttribute( "nillable"  , "true" );
    oNode.setAttribute( "name"      , "Value" );

    oInnerSeq.appendChild( oNode  );

    // ----------------------------------------------------------------------
    //<xs:element name="MapOfString<Type>" nillable="true" type="tns:MapOfString<Type>" /> 
    // ----------------------------------------------------------------------
    
    QDomElement oElementNode = createElement( "xs:element" );

    oElementNode.setAttribute( "type"    , "tns:" + sArrayName );
    oElementNode.setAttribute( "nillable", "true" );
    oElementNode.setAttribute( "name"    , sArrayName );

    // ----------------------------------------------------------------------
    // Build xml tree...
    // ----------------------------------------------------------------------

    appendChild( oRoot );

    if (bCustomType)
    {
        QDomElement oIncNode = createElement( "xs:include" );

        QString sBaseUri = "http://" + pRequest->m_mapHeaders[ "host" ] + pRequest->m_sResourceUrl + "?type=";

        oIncNode.setAttribute( "schemaLocation", sBaseUri + sClassName );

        oRoot.appendChild( oIncNode );
    }
    
    oRoot.appendChild( oTypeNode    );
    oRoot.appendChild( oElementNode );

    // ----------------------------------------------------------------------
    // Return xsd doc to caller
    // ----------------------------------------------------------------------

    QTextStream os( &(pRequest->m_response) );

    pRequest->m_eResponseType   = ResponseTypeXML;

    save( os, 0 );

    return true;

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QDomElement Xsd::CreateSchemaRoot()
{
    QString sTargetNamespace = "http://mythtv.org";
    
    QDomElement oRoot = createElement( "xs:schema" );

    oRoot.setAttribute( "xmlns:xs"            , "http://www.w3.org/2001/XMLSchema");
    oRoot.setAttribute( "xmlns:tns"           , sTargetNamespace );
    oRoot.setAttribute( "targetNamespace"     , sTargetNamespace );
    oRoot.setAttribute( "elementFormDefault"  , "qualified" );
    oRoot.setAttribute( "attributeFormDefault", "unqualified" );
    return oRoot;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Xsd::IsNillable( const QString &sType )
{
    if (sType.startsWith( "DTC::"))
        return true;

    if (sType == "QDateTime")
        return true;

    if (sType == "QDate")
        return true;

    if (sType == "QTime")
        return true;

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString Xsd::ConvertTypeToXSD( const QString &sType, bool bCustomType )
{
    if (bCustomType || sType.startsWith( "DTC::"))
    {
        QString sTypeName( sType );

        sTypeName.remove( "DTC::"    );
        sTypeName.remove( QChar('*') );

        if (sType == "QStringList" )
            return "ArrayOfString";   

        return sTypeName;
    }

    if (sType == "QDateTime")
        return "dateTime";

    if (sType == "QDate")
        return "date";

    if (sType == "QTime")
        return "time";

    if (sType == "bool")
        return "boolean";

    if (sType == "uint")
        return "unsignedInt";

    if (sType == "qlonglong" )
        return "long";

    if (sType == "long long" )
        return "long";

    if (sType == "QFileInfo" )
        return "string";        // temp solution

    if (sType.at(0) == 'Q')
        return sType.mid( 1 ).toLower();

    return sType.toLower();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString Xsd::ReadPropertyMetadata( QObject *pObject, QString sPropName, QString sKey )
{
    const QMetaObject *pMeta = pObject->metaObject();

    int nIdx = pMeta->indexOfClassInfo( sPropName.toUtf8() );

    if (nIdx >=0)
    {
        QString     sMetadata = pMeta->classInfo( nIdx ).value();
        QStringList sOptions  = sMetadata.split( ';' );

        QString     sFullKey  = sKey + "=";

        for (int nIdx = 0; nIdx < sOptions.size(); ++nIdx)
        {
            if (sOptions.at( nIdx ).startsWith( sFullKey ))
                return sOptions.at( nIdx ).mid( sFullKey.length() );
        }
    }

    return QString();
}
