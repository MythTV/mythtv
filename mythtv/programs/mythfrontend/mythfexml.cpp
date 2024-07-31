//////////////////////////////////////////////////////////////////////////////
// Program Name: MythFE.cpp
//
// Purpose - Frontend Html & XML status HttpServerExtension
//
//////////////////////////////////////////////////////////////////////////////

// Qt
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QKeyEvent>
#include <QTextStream>

// MythTV
#include "libmythbase/mythconfig.h"
#include "libmythbase/configuration.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdbcon.h"
#include "libmythui/mythmainwindow.h"

// MythFrontend
#include "keybindings.h"
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
    if (sURI == "GetRemote")     return MFEXML_GetRemote;

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
        case MFEXML_GetRemote:
            pRequest->FormatFileResponse(m_sSharePath + "html/frontend_index.qsp");
            break;
        default:
            UPnp::FormatErrorResponse(pRequest, UPnPResult_InvalidAction);
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

    QHashIterator<QString,QStringList> contexts(Frontend::gActionDescriptions);
    while (contexts.hasNext())
    {
        contexts.next();
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

static inline QString BUTTON(const char *action, const char *desc)
{
    return QString("      <input class=\"bigb\" type=\"button\" value=\"%1\" onClick=\"postaction('%2');\"></input>\r\n").arg(action, desc);
};

void MythFEXML::GetRemote(HTTPRequest *pRequest)
{
    pRequest->m_eResponseType = ResponseTypeHTML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    QTextStream stream( &pRequest->m_response );

    stream <<
        "<html>\n" << PROCESS_ACTION <<
        "  <style type=\"text/css\" title=\"Default\" media=\"all\">\r\n"
        "  <!--\r\n"
        "  body {\r\n"
        "    margin: 0px;\r\n"
        "    width : 310px;\r\n"
        "  }\r\n"
        "  -->\r\n"
        "  .bigb {\r\n"
        "    width : 100px;\r\n"
        "    height: 50px;\r\n"
        "    margin: 0px;\r\n"
        "    text-align: center;\r\n"
        "  }\r\n"
        "  </style>\r\n"
        "  <title>MythFrontend Control</title>\r\n" <<
        "  <body>\n" << HIDDEN_IFRAME;

    stream <<
        "    <div>\r\n" <<
        BUTTON("1","1") << BUTTON("2","2") << BUTTON("3","3") <<
        "    </div>\r\n" <<
        "    <div>\r\n" <<
        BUTTON("4","4") << BUTTON("5","5") << BUTTON("6","6") <<
        "    </div>\r\n" <<
        "    <div>\r\n" <<
        BUTTON("7","7") << BUTTON("8","8") << BUTTON("9","9") <<
        "    </div>\r\n" <<
        "    <div>\r\n" <<
        BUTTON("MENU","MENU") << BUTTON("0","0") << BUTTON("INFO","INFO") <<
        "    </div>\r\n" <<
        "    <div>\r\n" <<
        BUTTON("Back","ESCAPE") << BUTTON("^","UP") << BUTTON("MUTE","MUTE") <<
        "    </div>\r\n" <<
        "    <div>\r\n" <<
        BUTTON("<","LEFT") << BUTTON("Enter","SELECT") << BUTTON(">","RIGHT") <<
        "    </div>\r\n" <<
        "    <div>\r\n" <<
        BUTTON("<<","JUMPRWND") << BUTTON("v","DOWN") << BUTTON(">>","JUMPFFWD") <<
        "    </div>\r\n";

    stream <<
        "  </body>\n"
        "</html>\n";
}
