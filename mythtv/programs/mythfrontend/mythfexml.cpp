//////////////////////////////////////////////////////////////////////////////
// Program Name: MythFE.cpp
//
// Purpose - Frontend Html & XML status HttpServerExtension
//
//////////////////////////////////////////////////////////////////////////////

// Qt
#include <QTextStream>

// MythTV
#include "libmythbase/configuration.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"
#include "libmythui/mythmainwindow.h"
#include "libmythupnp/httprequest.h"
#include "libmythupnp/upnpresultcode.h"

// MythFrontend
#include "mythfexml.h"
#include "services/frontend.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MythFEXML::MythFEXML( UPnpDevice *pDevice , const QString &sSharePath)
  : Eventing( "MythFEXML", "MYTHTV_Event", sSharePath),
    m_sControlUrl("/MythFE")
{

    QString sUPnpDescPath = XmlConfiguration().GetValue("UPnP/DescXmlPath", m_sSharePath);

    m_sServiceDescFileName = sUPnpDescPath + "MFEXML_scpd.xml";

    // Add our Service Definition to the device.

    RegisterService( pDevice );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MythFEXMLMethod MythFEXML::GetMethod(const QString &sURI)
{
    if (sURI == "GetServDesc")   return MFEXML_GetServiceDescription;
    if (sURI == "GetScreenShot") return MFEXML_GetScreenShot;
    if (sURI == "GetActionTest") return MFEXML_ActionListTest;

    return( MFEXML_Unknown );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList MythFEXML::GetBasePaths()
{
    return Eventing::GetBasePaths() << m_sControlUrl;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool MythFEXML::ProcessRequest( HTTPRequest *pRequest )
{
    if (!pRequest)
        return false;

    if (pRequest->m_sBaseUrl != m_sControlUrl)
        return false;

    LOG(VB_UPNP, LOG_INFO, QString("MythFEXML::ProcessRequest: %1 : %2")
            .arg(pRequest->m_sMethod, pRequest->m_sRawRequest));

    switch(GetMethod(pRequest->m_sMethod))
    {
        case MFEXML_GetServiceDescription:
            pRequest->FormatFileResponse(m_sServiceDescFileName);
            break;
        case MFEXML_GetScreenShot:
            GetScreenShot(pRequest);
            break;
        case MFEXML_ActionListTest:
            GetActionListTest(pRequest);
            break;
        default:
            pRequest->FormatErrorResponse(UPnPResult_InvalidAction);
    }
    return true;
}

// ==========================================================================
// Request handler Methods
// ==========================================================================

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void MythFEXML::GetScreenShot(HTTPRequest *pRequest)
{
    pRequest->m_eResponseType = ResponseTypeFile;

    // Optional Parameters
    int     nWidth    = pRequest->m_mapParams[ "width"     ].toInt();
    int     nHeight   = pRequest->m_mapParams[ "height"    ].toInt();
    QString sFormat   = pRequest->m_mapParams[ "format"    ];

    if (sFormat.isEmpty())
        sFormat = "png";

    if (sFormat != "jpg" && sFormat != "png")
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid screen shot format: " + sFormat);
        return;
    }

    LOG(VB_GENERAL, LOG_INFO,
        QString("Screen shot requested (%1x%2), format %3")
            .arg(nWidth).arg(nHeight).arg(sFormat));

    QString sFileName = QString("/%1/myth-screenshot-XML.%2")
        .arg(gCoreContext->GetSetting("ScreenShotPath","/tmp"), sFormat);

    MythMainWindow *window = GetMythMainWindow();
    window->RemoteScreenShot(sFileName, nWidth, nHeight);

    pRequest->m_sFileName = sFileName;
}

static const QString PROCESS_ACTION =
    "  <script type =\"text/javascript\">\n"
    "    function postaction(action) {\n"
    "      var myForm = document.createElement(\"form\");\n"
    "      myForm.method =\"Post\";\n"
    "      myForm.action =\"../Frontend/SendAction?\";\n"
    "      myForm.target =\"post_target\";\n"
    "      var myInput = document.createElement(\"input\");\n"
    "      myInput.setAttribute(\"name\", \"Action\");\n"
    "      myInput.setAttribute(\"value\", action);\n"
    "      myForm.appendChild(myInput);\n"
    "      document.body.appendChild(myForm);\n"
    "      myForm.submit();\n"
    "      document.body.removeChild(myForm);\n"
    "    }\n"
    "  </script>\n";

static const QString HIDDEN_IFRAME =
    "    <iframe id=\"hidden_target\" name=\"post_target\" src=\"\""
    " style=\"width:0;height:0;border:0px solid #fff;\"></iframe>\n";

void MythFEXML::GetActionListTest(HTTPRequest *pRequest)
{
    Frontend::InitialiseActions();

    pRequest->m_eResponseType = ResponseTypeHTML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    QTextStream stream( &pRequest->m_response );

    stream <<
        "<html>\n" << PROCESS_ACTION <<
        "  <body>\n" << HIDDEN_IFRAME;

    for (auto contexts = Frontend::gActionDescriptions.cbegin();
         contexts != Frontend::gActionDescriptions.cend(); ++contexts)
    {
        QStringList actions = contexts.value();
        for (const QString & action : std::as_const(actions))
        {
            QStringList split = action.split(",");
            if (split.size() == 2)
            {
                stream <<
                    QString("    <div>%1&nbsp;<input type=\"button\" value=\"%2\" onClick=\"postaction('%2');\"></input>&nbsp;%3</div>\n")
                        .arg(contexts.key(), split[0], split[1]);
            }
        }
    }

    stream <<
        "  </body>\n"
        "</html>\n";

}
