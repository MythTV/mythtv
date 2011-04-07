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

#include <QTextStream>
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

    QString sUPnpDescPath = UPnp::GetConfiguration()->GetValue( "UPnP/DescXmlPath", m_sSharePath );

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
        case MFEXML_GetRemote:
            GetRemote(pRequest);
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
    pRequest->m_eResponseType = ResponseTypeHTML;

    // Optional Parameters
    int     nWidth    = pRequest->m_mapParams[ "width"     ].toInt();
    int     nHeight   = pRequest->m_mapParams[ "height"    ].toInt();
   
    VERBOSE(VB_GENERAL, QString("Screen shot requested (%1x%2)")
        .arg(nWidth).arg(nHeight));

    MythMainWindow *window = GetMythMainWindow();
    QStringList args;
    if (nWidth && nHeight)
    {
        args << QString::number(nWidth);
        args << QString::number(nHeight);
    }
    MythEvent* me = new MythEvent(MythEvent::MythEventMessage,
                                  ACTION_SCREENSHOT, args);
    qApp->postEvent(window, me);
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

static const QString PROCESS_ACTION =
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
    "  </script>\n";

static const QString HIDDEN_IFRAME =
    "    <iframe id=\"hidden_target\" name=\"post_target\" src=\"\""
    " style=\"width:0;height:0;border:0px solid #fff;\"></iframe>\n";

void MythFEXML::GetActionListTest(HTTPRequest *pRequest)
{
    InitActions();

    pRequest->m_eResponseType = ResponseTypeHTML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    QTextStream stream( &pRequest->m_response );

    stream <<
        "<html>\n" << PROCESS_ACTION <<
        "  <body>\n" << HIDDEN_IFRAME;

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
                stream <<
                    QString("    <div>%1&nbsp;<input type=\"button\" value=\"%2\" onClick=\"postaction('%2');\"></input>&nbsp;%3</div>\n")
                        .arg(contexts.key()).arg(split[0]).arg(split[1]);
            }
        }
    }

    stream <<
        "  </body>\n"
        "</html>\n";

}

void MythFEXML::GetActionList(HTTPRequest *pRequest)
{
    InitActions();

    pRequest->m_eResponseType = ResponseTypeXML;
    pRequest->m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";

    QTextStream stream( &pRequest->m_response );

    stream << "<mythactions version=\"1\">";

    QHashIterator<QString,QStringList> contexts(m_actionDescriptions);
    while (contexts.hasNext())
    {
        contexts.next();
        stream << QString("<context name=\"%1\">")
                                        .arg(contexts.key());
        QStringList actions = contexts.value();
        foreach (QString action, actions)
        {
            QStringList split = action.split(",");
            if (split.size() == 2)
            {
                stream <<
                    QString("<action name=\"%1\">%2</action>")
                    .arg(split[0]).arg(split[1]);
            }
        }
        stream << "</context>";
    }
    stream << "</mythactions>";
}

#define BUTTON(action,desc) \
  QString("      <input class=\"bigb\" type=\"button\" value=\"%1\" onClick=\"postaction('%2');\"></input>\r\n").arg(action).arg(desc)

void MythFEXML::GetRemote(HTTPRequest *pRequest)
{
    InitActions();

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
