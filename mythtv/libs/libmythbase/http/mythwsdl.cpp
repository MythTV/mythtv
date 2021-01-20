// MythTV
#include "http/mythmimedatabase.h"
#include "http/mythhttpmetaservice.h"
#include "http/mythhttprequest.h"
#include "http/mythhttpresponse.h"
#include "http/mythwsdl.h"

HTTPResponse MythWSDL::GetWSDL(HTTPRequest2 Request, MythHTTPMetaService *MetaService)
{
    MythWSDL wsdl(Request, MetaService);
    return wsdl.m_result;
}

MythWSDL::MythWSDL(HTTPRequest2 Request, MythHTTPMetaService *MetaService)
{
    if (!(Request && MetaService))
        return;

    if (!Request->m_queries.contains("raw"))
        appendChild(createProcessingInstruction("xml-stylesheet", R"(type="text/xsl" href="/xslt/service.xslt")"));

    QString sClassName = MetaService->m_name;
    QString sTargetNamespace = "http://mythtv.org";
    m_oRoot = createElementNS("http://schemas.xmlsoap.org/wsdl/", "definitions");
    m_oRoot.setAttribute("targetNamespace", sTargetNamespace );
    m_oRoot.setAttribute("xmlns:soap", "http://schemas.xmlsoap.org/wsdl/soap/" );
    m_oRoot.setAttribute("xmlns:xs"  , "http://www.w3.org/2001/XMLSchema"      );
    m_oRoot.setAttribute("xmlns:soap", "http://schemas.xmlsoap.org/wsdl/soap/" );
    m_oRoot.setAttribute("xmlns:tns" , sTargetNamespace );
    m_oRoot.setAttribute("xmlns:wsaw", "http://www.w3.org/2006/05/addressing/wsdl" );
    m_oRoot.setAttribute("name", QString("%1Services" ).arg(sClassName ) );
    m_oTypes    = createElement("types"    );
    m_oLastMsg  = m_oTypes;
    m_oPortType = createElement("portType" );
    m_oBindings = createElement("binding"  );
    m_oService  = createElement("service"  );
    appendChild(m_oRoot      );
    m_oRoot.appendChild(m_oTypes    );
    m_oRoot.appendChild(m_oPortType );
    m_oRoot.appendChild(m_oBindings );
    m_oRoot.appendChild(m_oService  );
    m_oPortType.setAttribute("name", sClassName );
    QDomElement oImportNode = createElement( "xs:schema" );
    oImportNode.setAttribute( "targetNamespace"   , "http://MythTV.org/Imports" );
    m_oTypes.appendChild( oImportNode );

    QDomElement oNode = createElement( "xs:schema" );
    oNode.setAttribute( "targetNamespace"   , sTargetNamespace );
    oNode.setAttribute( "elementFormDefault", "qualified"      );
    m_oTypes.appendChild( oNode );
    m_oTypes = oNode;

    // Add Bindings
    m_oBindings.setAttribute( "name", QString("BasicHttpBinding_%1").arg( sClassName ));
    m_oBindings.setAttribute( "type", QString( "tns:%1"            ).arg( sClassName ));
    oNode = createElement( "soap:binding" );
    //oNode.setAttribute( "style"    , "document" );
    oNode.setAttribute( "transport", "http://schemas.xmlsoap.org/soap/http" );
    m_oBindings.appendChild( oNode );

    // Create the XML result
    auto data = MythHTTPData::Create(toByteArray());
    data->m_mimeType = MythMimeDatabase().MimeTypeForName("application/xml");
    data->m_cacheType = HTTPETag | HTTPShortLife;
    m_result = MythHTTPResponse::DataResponse(Request, data);
}


