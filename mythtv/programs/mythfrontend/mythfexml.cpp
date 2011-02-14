//////////////////////////////////////////////////////////////////////////////
// Program Name: MythFE.cpp
//                                                                            
// Purpose - Frontend Html & XML status HttpServerExtension
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "mythfexml.h"

#include "mythcorecontext.h"
#include "util.h"
#include "mythdbcon.h"

#include "mythmainwindow.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QDir>
#include <QFile>
#include <QRegExp>
#include <QBuffer>
#include <QKeyEvent>

#include "../../config.h"

#include "keybindings.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MythFEXML::MythFEXML( UPnpDevice *pDevice , const QString sSharePath)
  : Eventing( "MythFEXML", "MYTHTV_Event", sSharePath)
{

    QString sUPnpDescPath = UPnp::g_pConfig->GetValue( "UPnP/DescXmlPath", m_sSharePath );

    m_sServiceDescFileName = sUPnpDescPath + "MFEXML_scpd.xml";
    m_sControlUrl          = "/MythFE";

    // Add our Service Definition to the device.

    RegisterService( pDevice );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MythFEXML::~MythFEXML()
{
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MythFEXMLMethod MythFEXML::GetMethod(const QString &sURI)
{
    if (sURI == "GetServDesc")   return MFEXML_GetServiceDescription;
    if (sURI == "GetScreenShot") return MFEXML_GetScreenShot;
    if (sURI == "SendMessage")   return MFEXML_Message;
    if (sURI == "SendAction")    return MFEXML_Action;
    if (sURI == "GetActionList") return MFEXML_ActionList;
    if (sURI == "GetActionTest") return MFEXML_ActionListTest;

    return( MFEXML_Unknown );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool MythFEXML::ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest )
{
    if (!pRequest)
        return false;

    if (pRequest->m_sBaseUrl != m_sControlUrl)
        return false;

    VERBOSE(VB_UPNP, QString("MythFEXML::ProcessRequest: %1 : %2")
            .arg(pRequest->m_sMethod).arg(pRequest->m_sRawRequest));

    switch(GetMethod(pRequest->m_sMethod))
    {
        case MFEXML_GetServiceDescription:
            pRequest->FormatFileResponse(m_sServiceDescFileName);
            break;
        case MFEXML_GetScreenShot:
            GetScreenShot(pRequest);
            break;
        case MFEXML_Message:
            SendMessage(pRequest);
            break;
        case MFEXML_Action:
            SendAction(pRequest);
            break;
        case MFEXML_ActionList:
            GetActionList(pRequest);
            break;
        case MFEXML_ActionListTest:
            GetActionListTest(pRequest);
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
    pRequest->m_eResponseType   = ResponseTypeFile;

    // Optional Parameters

    int     nWidth    = pRequest->m_mapParams[ "width"     ].toInt();
    int     nHeight   = pRequest->m_mapParams[ "height"    ].toInt();
    QString sFormat   = pRequest->m_mapParams[ "format"    ].toLower();

    if (sFormat.isEmpty())
    {
        sFormat = "png";
    }

    if (sFormat != "jpg" && sFormat != "png" && sFormat != "gif") {
        VERBOSE(VB_GENERAL, QString("Invalid screen shot format: %1") 
            .arg(sFormat));
        return;
    }
   
    VERBOSE(VB_GENERAL, QString("Screen shot requested - %1") .arg(sFormat));

    QString sFileName = QString("/%1/myth-screenshot-XML.%2")
                    .arg(gCoreContext->GetSetting("ScreenShotPath","/tmp/"))
                    .arg(sFormat);

    MythMainWindow *window = GetMythMainWindow();
    emit window->remoteScreenShot(sFileName, nWidth, nHeight);

    pRequest->m_sFileName = sFileName;
}

void MythFEXML::SendMessage(HTTPRequest *pRequest)
{
    pRequest->m_eResponseType = ResponseTypeHTML;
    QString sText = pRequest->m_mapParams[ "text" ];
    VERBOSE(VB_GENERAL, QString("UPNP message: ") + sText);

    MythMainWindow *window = GetMythMainWindow();
    MythEvent* me = new MythEvent(MythEvent::MythUserMessage, sText);
    qApp->postEvent(window, me);
}

void MythFEXML::SendAction(HTTPRequest *pRequest)
{
    pRequest->m_eResponseType = ResponseTypeHTML;
    QString sText = pRequest->m_mapParams["action"];
    VERBOSE(VB_UPNP, QString("UPNP Action: ") + sText);

    InitActions();
    if (!m_actionList.contains(sText))
    {
        VERBOSE(VB_GENERAL, QString("UPNP Action: %1 is invalid").arg(sText));
        return;
    }

    MythMainWindow *window = GetMythMainWindow();
    QKeyEvent* ke = new QKeyEvent(QEvent::KeyPress, 0, Qt::NoModifier, sText);
    qApp->postEvent(window, (QEvent*)ke);
}

void MythFEXML::GetActionListTest(HTTPRequest *pRequest)
{
    InitActions();

    pRequest->m_eResponseType = ResponseTypeHTML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    pRequest->m_response <<
        "<html>\n"
        "  <script type =\"text/javascript\">\n"
        "    function postaction(action) {\n"
        "      var myForm = document.createElement(\"form\");\n"
        "      myForm.method =\"Post\";\n"
        "      myForm.action =\"SendAction?\";\n"
        "      myForm.target =\"post_target\";\n"
        "      var myInput = document.createElement(\"input\");\n"
        "      myInput.setAttribute(\"name\", \"action\");\n"
        "      myInput.setAttribute(\"value\", action);\n"
        "      myForm.appendChild(myInput);\n"
        "      document.body.appendChild(myForm);\n"
        "      myForm.submit();\n"
        "      document.body.removeChild(myForm);\n"
        "    }\n"
        "  </script>\n"
        "  <body>\n"
        "    <iframe id=\"hidden_target\" name=\"post_target\" src=\"\""
        " style=\"width:0;height:0;border:0px solid #fff;\"></iframe>\n";

    QHashIterator<QString,QStringList> contexts(m_actionDescriptions);
    while (contexts.hasNext())
    {
        contexts.next();
        QStringList actions = contexts.value();
        foreach (QString action, actions)
        {
            QStringList split = action.split(",");
            if (split.size() == 2)
            {
                pRequest->m_response <<
                    QString("    <div>%1&nbsp;<input type=\"button\" value=\"%2\" onClick=\"postaction('%2');\"></input>&nbsp;%3</div>\n")
                        .arg(contexts.key()).arg(split[0]).arg(split[1]);
            }
        }
    }

    pRequest->m_response <<
        "  </body>\n"
        "</html>\n";

}

void MythFEXML::GetActionList(HTTPRequest *pRequest)
{
    InitActions();

    pRequest->m_eResponseType = ResponseTypeXML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    pRequest->m_response << "<mythactions version=\"1\">";

    QHashIterator<QString,QStringList> contexts(m_actionDescriptions);
    while (contexts.hasNext())
    {
        contexts.next();
        pRequest->m_response << QString("<context name=\"%1\">")
                                        .arg(contexts.key());
        QStringList actions = contexts.value();
        foreach (QString action, actions)
        {
            QStringList split = action.split(",");
            if (split.size() == 2)
            {
                pRequest->m_response <<
                    QString("<action name=\"%1\">%2</action>")
                    .arg(split[0]).arg(split[1]);
            }
        }
        pRequest->m_response << "</context>";
    }
    pRequest->m_response << "</mythactions>";
}

void MythFEXML::InitActions(void)
{
    static bool initialised = false;
    if (initialised)
        return;

    initialised = true;
    KeyBindings *bindings = new KeyBindings(gCoreContext->GetHostName());
    if (bindings)
    {
        QStringList contexts = bindings->GetContexts();
        contexts.sort();
        foreach (QString context, contexts)
        {
            m_actionDescriptions[context] = QStringList();
            QStringList ctx_actions = bindings->GetActions(context);
            ctx_actions.sort();
            m_actionList += ctx_actions;
            foreach (QString actions, ctx_actions)
            {
                QString desc = actions + "," +
                               bindings->GetActionDescription(context, actions);
                m_actionDescriptions[context].append(desc);
            }
        }
    }
    m_actionList.removeDuplicates();
    m_actionList.sort();

    foreach (QString actions, m_actionList)
        VERBOSE(VB_UPNP, QString("MythFEXML Action: %1").arg(actions));
}
