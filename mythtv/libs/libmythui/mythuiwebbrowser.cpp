/**
 * \file mythuiwebbrowser.cpp
 * \author Paul Harrison <pharrison@mythtv.org>
 * \brief Provide a web browser widget.
 */

#include "mythuiwebbrowser.h"

// c++
#include <algorithm>
#include <chrono> // for milliseconds
#include <thread> // for sleep_for

// qt
#include <QApplication>
#include <QWebEngineHistory>
#include <QWebEngineSettings>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QPainter>
#include <QDir>
#include <QStyle>
#include <QKeyEvent>
#include <QDomDocument>
#include <QDebug>

// libmythbase
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythdownloadmanager.h"
#include "libmythbase/mythlogging.h"

//libmythui
#include "mythpainter.h"
#include "mythimage.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"
#include "mythuihelper.h"
#include "mythdialogbox.h"
#include "mythprogressdialog.h"
#include "mythuiscrollbar.h"
#include <qlogging.h>

struct MimeType
{
    QString m_mimeType;
    QString m_extension;
    bool    m_isVideo;
};

static const std::vector<MimeType> SupportedMimeTypes
{
    { "audio/mpeg3",                 "mp3",   false },
    { "audio/x-mpeg-3",              "mp3",   false },
    { "audio/mpeg",                  "mp2",   false },
    { "audio/x-mpeg",                "mp2",   false },
    { "audio/ogg",                   "ogg",   false },
    { "audio/ogg",                   "oga",   false },
    { "audio/flac",                  "flac",  false },
    { "audio/x-ms-wma",              "wma",   false },
    { "audio/wav",                   "wav",   false },
    { "audio/x-wav",                 "wav",   false },
    { "audio/ac3",                   "ac3",   false },
    { "audio/x-ac3",                 "ac3",   false },
    { "audio/x-oma",                 "oma",   false },
    { "audio/x-realaudio",           "ra",    false },
    { "audio/dts",                   "dts",   false },
    { "audio/x-dts",                 "dts",   false },
    { "audio/aac",                   "aac",   false },
    { "audio/x-aac",                 "aac",   false },
    { "audio/m4a",                   "m4a",   false },
    { "audio/x-m4a",                 "m4a",   false },
    { "video/mpeg",                  "mpg",   true },
    { "video/mpeg",                  "mpeg",  true },
    { "video/x-ms-wmv",              "wmv",   true },
    { "video/x-ms-wmv",              "avi",   true },
    { "application/x-troff-msvideo", "avi",   true },
    { "video/avi",                   "avi",   true },
    { "video/msvideo",               "avi",   true },
    { "video/x-msvideo",             "avi",   true }
};


/**
 * @class MythWebEngineView
 * @brief Subclass of QWebEngineView
 * \note allows us to intercept keypresses
 */
MythWebEngineView::MythWebEngineView(QWidget *parent, MythUIWebBrowser *parentBrowser)
            : QWebEngineView(parent), m_parentBrowser(parentBrowser)
{
    m_profile = new QWebEngineProfile("MythTV", this);
    //m_profile->setHttpUserAgent("Mozilla/5.0 (SMART-TV; Linux; Tizen 5.0) AppleWebKit/538.1 (KHTML, like Gecko) Version/5.0 NativeTVAds Safari/538.1");
    m_profile->setPersistentCookiesPolicy(QWebEngineProfile::AllowPersistentCookies);

    m_webpage = new QWebEnginePage(m_profile, this);

    setPage(m_webpage);

    installEventFilter(this);
}

MythWebEngineView::~MythWebEngineView(void)
{
    delete m_webpage;
    delete m_profile;
}

bool MythWebEngineView::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::ShortcutOverride) 
    {
        // intercept all key presses
        auto *keyEvent = dynamic_cast<QKeyEvent *>(event);
        if (keyEvent == nullptr)
        {
            LOG(VB_GENERAL, LOG_ALERT,
                     "MythWebEngineView::eventFilter() couldn't cast event");
            return true;
        }

        bool res = handleKeyPress(keyEvent);
        if (!res)
            keyEvent->accept();
        else
            keyEvent->ignore();

        return false; // clazy:exclude=base-class-event
    }

    // standard event processing
    return QWebEngineView::eventFilter(obj, event);
}

void MythWebEngineView::sendKeyPress(int key, Qt::KeyboardModifiers modifiers, const QString &text)
{
    for (QObject* obj : children())
    {
        if (qobject_cast<QWidget*>(obj))
        {
            auto *event = new QKeyEvent(QEvent::KeyPress, key, modifiers, text, false, 1);
            QCoreApplication::postEvent(obj, event);
        }
    }
}

bool MythWebEngineView::handleKeyPress(QKeyEvent *event)
{
    QStringList actions;
    bool handled = true;
    handled = GetMythMainWindow()->TranslateKeyPress("Browser", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "ESCAPE")
        {
            if (history()->canGoBack())
                back();
            else
            {
                m_parentBrowser->GetParentScreen()->keyPressEvent(event);
                handled = true;
            }
        }
        else if (action == "NEXTLINK")
        {
            if (event->key() != Qt::Key_Tab)
            {
                sendKeyPress(Qt::Key_Tab, Qt::NoModifier, QString('\t'));
                return true;
            }

            handled = false;
        }
        else if (action == "PREVIOUSLINK")
        {
            if (event->key() != Qt::Key_Tab)
            {
                sendKeyPress(Qt::Key_Tab, Qt::ShiftModifier, QString('\t'));
                return true;
            }

            handled = false;
        }
        else if (action == "FOLLOWLINK")
        {
            if (event->key() != Qt::Key_Return)
            {
                sendKeyPress(Qt::Key_Return, Qt::NoModifier, QString('\r'));
                return true;
            }

            handled = false;
        }
        else if (action == "TOGGLEINPUT")
        {
            if (m_parentBrowser->IsInputToggled())
            {
                m_parentBrowser->SetInputToggled(false);
                m_parentBrowser->SendStatusBarMessage(tr("Sending key presses to MythTV"));
            }
            else
            {
                m_parentBrowser->SetInputToggled(true);
                m_parentBrowser->SendStatusBarMessage(tr("Sending key presses to web page"));
            }
        }
        else if (action == "ZOOMIN")
        {
            m_parentBrowser->ZoomIn();
        }
        else if (action == "ZOOMOUT")
        {
            m_parentBrowser->ZoomOut();
        }
        else if (action == "RELOAD")
        {
            m_parentBrowser->Reload(true);
        }
        else if (action == "FULLRELOAD")
        {
            m_parentBrowser->Reload(false);
        }
        else if (action == "MOUSEUP" || action == "MOUSEDOWN" ||
                action == "MOUSELEFT" || action == "MOUSERIGHT" ||
                action == "MOUSELEFTBUTTON")
        {
            m_parentBrowser->HandleMouseAction(action);
        }
        else if (action == "HISTORYBACK")
        {
            m_parentBrowser->Back();
        }
        else if (action == "HISTORYFORWARD")
        {
            m_parentBrowser->Forward();
        }
        else
        {
            handled = false;
        }
    }

    return handled;
}

void MythWebEngineView::openBusyPopup(const QString &message)
{
    if (m_busyPopup)
        return;

    QString msg(tr("Downloading..."));

    if (!message.isEmpty())
        msg = message;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    m_busyPopup = new MythUIBusyDialog(msg, popupStack, "downloadbusydialog");

    if (m_busyPopup->Create())
        popupStack->AddScreen(m_busyPopup, false);
}

void MythWebEngineView::closeBusyPopup(void)
{
    if (m_busyPopup)
        m_busyPopup->Close();

    m_busyPopup = nullptr;
}

void MythWebEngineView::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = (DialogCompletionEvent *)(event);

        // make sure the user didn't ESCAPE out of the dialog
        if (dce->GetResult() < 0)
            return;

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();

        if (resultid == "filenamedialog")
        {
            //doDownload(resulttext);
        }
        else if (resultid == "downloadmenu")
        {
            if (resulttext == tr("Play the file"))
            {
                QFileInfo fi(m_downloadRequest.url().path());
                QString extension = fi.suffix();
                QString mimeType = getReplyMimetype();

                if (isMusicFile(extension, mimeType))
                {
                    MythEvent me(QString("MUSIC_COMMAND %1 PLAY_URL %2")
                                 .arg(gCoreContext->GetHostName(),
                                      m_downloadRequest.url().toString()));
                    gCoreContext->dispatch(me);
                }
                else if (isVideoFile(extension, mimeType))
                {
                    GetMythMainWindow()->HandleMedia("Internal",
                                            m_downloadRequest.url().toString());
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("MythWebEngineView: Asked to play a file with "
                                "extension '%1' but don't know how")
                        .arg(extension));
                }
            }
            else if (resulttext == tr("Download the file"))
            {
                //doDownloadRequested(m_downloadRequest);
            }
            else if (resulttext == tr("Download and play the file"))
            {
                m_downloadAndPlay = true;
                //doDownloadRequested(m_downloadRequest);
            }
        }
    }
    else if (event->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(event);
        if (me == nullptr)
            return;

        QStringList tokens = me->Message().split(" ", Qt::SkipEmptyParts);
        if (tokens.isEmpty())
            return;

        if (tokens[0] == "DOWNLOAD_FILE")
        {
            QStringList args = me->ExtraDataList();

            if (tokens[1] == "UPDATE")
            {
                // could update a progressbar here
            }
            else if (tokens[1] == "FINISHED")
            {
                int fileSize  = args[2].toInt();
                int errorCode = args[4].toInt();
                const QString& filename = args[1];

                closeBusyPopup();

                if ((errorCode != 0) || (fileSize == 0))
                    ShowOkPopup(tr("ERROR downloading file."));
                else if (m_downloadAndPlay)
                    GetMythMainWindow()->HandleMedia("Internal", filename);

                MythEvent me2(QString("BROWSER_DOWNLOAD_FINISHED"), args);
                gCoreContext->dispatch(me2);
            }
        }
    }

    if (event->type() == QEvent::UpdateRequest)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("MythWebEngineView: customeEvent: QEvent::UpdateRequest"));
        m_parentBrowser->UpdateBuffer();
    }
}

void MythWebEngineView::showDownloadMenu(void)
{
    QFileInfo fi(m_downloadRequest.url().path());
    QString extension = fi.suffix();
    QString mimeType = getReplyMimetype();

    QString label = tr("What do you want to do with this file?");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menu = new MythDialogBox(label, popupStack, "downloadmenu");

    if (!menu->Create())
    {
        delete menu;
        return;
    }

    menu->SetReturnEvent(this, "downloadmenu");

    if (isMusicFile(extension, mimeType))
        menu->AddButton(tr("Play the file"));

    if (isVideoFile(extension, mimeType))
        menu->AddButton(tr("Download and play the file"));

    menu->AddButton(tr("Download the file"));
    menu->AddButton(tr("Cancel"));

    popupStack->AddScreen(menu);
}

QString MythWebEngineView::getExtensionForMimetype(const QString &mimetype)
{
    if (mimetype.isEmpty())
        return {""};

    auto it = std::find_if(SupportedMimeTypes.cbegin(), SupportedMimeTypes.cend(),
                           [mimetype] (const MimeType& entry) -> bool
                               { return mimetype == entry.m_mimeType; });
    if (it != SupportedMimeTypes.cend())
        return it->m_extension;
    return {""};
}

bool MythWebEngineView::isMusicFile(const QString &extension, const QString &mimetype)
{
    return std::any_of(SupportedMimeTypes.cbegin(), SupportedMimeTypes.cend(),
                       [extension, mimetype](const auto &entry){
                           if (entry.m_isVideo)
                               return false;
                           if (!mimetype.isEmpty() &&
                               mimetype == entry.m_mimeType)
                               return true;
                           if (!extension.isEmpty() &&
                               extension.toLower() == entry.m_extension)
                               return true;
                           return false; } );
}

bool MythWebEngineView::isVideoFile(const QString &extension, const QString &mimetype)
{
    return std::any_of(SupportedMimeTypes.cbegin(), SupportedMimeTypes.cend(),
                       [extension, mimetype](const auto &entry) {
                           if (!entry.m_isVideo)
                               return false;
                           if (!mimetype.isEmpty() &&
                               mimetype == entry.m_mimeType)
                               return true;
                           if (!extension.isEmpty() &&
                               extension.toLower() == entry.m_extension)
                               return true;
                           return false; } );
}

QString MythWebEngineView::getReplyMimetype(void)
{
    if (!m_downloadReply)
        return {};

    QString mimeType;
    QVariant header = m_downloadReply->header(QNetworkRequest::ContentTypeHeader);

    if (header != QVariant())
        mimeType = header.toString();

    return mimeType;
}

QWebEngineView *MythWebEngineView::createWindow(QWebEnginePage::WebWindowType /* type */)
{
    return (QWebEngineView *) this;
}


/**
 * \class MythUIWebBrowser
 * \brief Provide a web browser widget.
 *
 * This widget can display HTML documents from the net, a file or passed to it
 * as a QString.
 *
 * This is how you would add the widget to a theme file :-
 *
 *
 *      <webbrowser name="webbrowser">
 *           <url>http://www.google.com/</url>
 *           <area>20,55,760,490</area>
 *           <zoom>1.4</zoom>
 *           <background color="white"/>
 *      </webbrowser>
 *
 * area is the screen area the widget should use.
 * zoom is the initial text size.
 * background is the default background color to use.
 *
 * The widget has two modes of operation active and inactive. When it's active
 * and has focus it will show the mouse pointer, links will respond when
 * clicked, the highlighed link can be changed and activated using the keyboard.
 * In inactive mode it will just display a static image of the web document,
 * you can scroll it but that is all.
 *
 * One thing to be aware of is you cannot show popups above this widget when it
 * is in active mode and has focus so either call Active(false) or move the
 * focus to another widget before showing the popup.
*/



/** \fn MythUIWebBrowser::MythUIWebBrowser(MythUIType*, const QString&)
 *  \brief the classes constructor
 *  \param parent the parent of this widget
 *  \param name the name of this widget
 */
MythUIWebBrowser::MythUIWebBrowser(MythUIType *parent, const QString &name)
                 : MythUIType(parent, name),
      m_updateInterval(500), m_bgColor("Red"),  m_userCssFile(""),
      m_defaultSaveDir(GetConfDir() + "/MythBrowser/"),
      m_defaultSaveFilename(""), m_lastMouseAction("")
{
    SetCanTakeFocus(true);
    m_lastUpdateTime.start();
}

/**
 *  \copydoc MythUIType::Finalize()
 */
void MythUIWebBrowser::Finalize(void)
{
    MythUIType::Finalize();

    Init();
}

/** \fn MythUIWebBrowser::Init(void)
 *  \brief Initializes the widget ready for use
 *  \note This is usually called for you when the widget is finalized
 *        during the theme file parsing but if you manually add this widget
 *        to a screen you will have to call this function *after* setting the
 *        widgets screen area.
 */
void MythUIWebBrowser::Init(void)
{
    // only do the initialisation for widgets not being stored in the global object store
    if (parent() == GetGlobalObjectStore())
        return;

    if (m_initialized)
        return;

    m_actualBrowserArea = m_browserArea;
    m_actualBrowserArea.CalculateArea(m_area);
    m_actualBrowserArea.translate(m_area.x(), m_area.y());

    if (!m_actualBrowserArea.isValid())
        m_actualBrowserArea = m_area;

    m_webEngine = new MythWebEngineView(GetMythMainWindow()->GetPaintWindow(), this);
    m_webEngine->setPalette(QApplication::style()->standardPalette());
    m_webEngine->setGeometry(m_actualBrowserArea);
    m_webEngine->setFixedSize(m_actualBrowserArea.size());
    m_webEngine->move(m_actualBrowserArea.x(), m_actualBrowserArea.y());
    m_webEngine->show();
    m_webEngine->raise();
    m_webEngine->setFocus();
    m_webEngine->update();

    SetRedraw();

    bool err = false;
    UIUtilW::Assign(this, m_horizontalScrollbar, "horizscrollbar", &err);
    UIUtilW::Assign(this, m_verticalScrollbar, "vertscrollbar", &err);

    if (m_horizontalScrollbar)
    {
        connect(m_horizontalScrollbar, &MythUIType::Hiding,
                this, &MythUIWebBrowser::slotScrollBarHiding);
        connect(m_horizontalScrollbar, &MythUIType::Showing,
                this, &MythUIWebBrowser::slotScrollBarShowing);
    }

    if (m_verticalScrollbar)
    {
        connect(m_verticalScrollbar, &MythUIType::Hiding,
                this, &MythUIWebBrowser::slotScrollBarHiding);
        connect(m_verticalScrollbar, &MythUIType::Showing,
                this, &MythUIWebBrowser::slotScrollBarShowing);
    }

    // if we have a valid css URL use that ...
    if (!m_userCssFile.isEmpty())
    {
        QString filename = m_userCssFile;

        if (GetMythUI()->FindThemeFile(filename))
            LoadUserStyleSheet(QUrl("file://" + filename));
    }
    else
    {
        // ...otherwise use the default one
        QString filename = "htmls/mythbrowser.css";

        if (GetMythUI()->FindThemeFile(filename))
            LoadUserStyleSheet(QUrl("file://" + filename));
    }

    SetHttpUserAgent("Mozilla/5.0 (SMART-TV; Linux; Tizen 5.0) AppleWebKit/538.1 (KHTML, like Gecko) Version/5.0 NativeTVAds Safari/538.1");

    connect(m_webEngine, &QWebEngineView::loadStarted, this, &MythUIWebBrowser::slotLoadStarted);
    connect(m_webEngine, &QWebEngineView::loadFinished, this, &MythUIWebBrowser::slotLoadFinished);
    connect(m_webEngine, &QWebEngineView::loadProgress, this, &MythUIWebBrowser::slotLoadProgress);
    connect(m_webEngine, &QWebEngineView::titleChanged, this, &MythUIWebBrowser::slotTitleChanged);
    connect(m_webEngine, &QWebEngineView::iconChanged, this, &MythUIWebBrowser::slotIconChanged);
    connect(m_webEngine, &QWebEngineView::iconUrlChanged, this, &MythUIWebBrowser::slotIconUrlChanged);
    connect(m_webEngine, &QWebEngineView::renderProcessTerminated, this, &MythUIWebBrowser::slotRenderProcessTerminated);

    connect(m_webEngine->page(), &QWebEnginePage::contentsSizeChanged, this, &MythUIWebBrowser::slotContentsSizeChanged);
    connect(m_webEngine->page(), &QWebEnginePage::scrollPositionChanged, this, &MythUIWebBrowser::slotScrollPositionChanged);
    connect(m_webEngine->page(), &QWebEnginePage::linkHovered, this, &MythUIWebBrowser::slotStatusBarMessage);
    connect(m_webEngine->page(), &QWebEnginePage::fullScreenRequested, this, &MythUIWebBrowser::slotFullScreenRequested);
    connect(m_webEngine->page(), &QWebEnginePage::windowCloseRequested, this, &MythUIWebBrowser::slotWindowCloseRequested);

    connect(this, &MythUIType::TakingFocus, this, &MythUIWebBrowser::slotTakingFocus);
    connect(this, &MythUIType::LosingFocus, this, &MythUIWebBrowser::slotLosingFocus);

    // find what screen we are on
    m_parentScreen = nullptr;
    QObject *parentObject = parent();

    while (parentObject)
    {
        m_parentScreen = qobject_cast<MythScreenType *>(parentObject);

        if (m_parentScreen)
            break;

        parentObject = parentObject->parent();
    }

    if (!m_parentScreen && parent() != GetGlobalObjectStore())
        LOG(VB_GENERAL, LOG_ERR, "MythUIWebBrowser: failed to find our parent screen");

    // connect to the topScreenChanged signals on each screen stack
    for (int x = 0; x < GetMythMainWindow()->GetStackCount(); x++)
    {
        MythScreenStack *stack = GetMythMainWindow()->GetStackAt(x);

        if (stack)
            connect(stack, &MythScreenStack::topScreenChanged, this, &MythUIWebBrowser::slotTopScreenChanged);
    }

    if (gCoreContext->GetNumSetting("WebBrowserEnablePlugins", 1) == 1)
    {
        LOG(VB_GENERAL, LOG_INFO, "MythUIWebBrowser: enabling plugins");
        m_webEngine->page()->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, "MythUIWebBrowser: disabling plugins");
        m_webEngine->page()->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    }

    if (!gCoreContext->GetBoolSetting("WebBrowserEnableJavascript", true))
    {
        LOG(VB_GENERAL, LOG_INFO, "MythUIWebBrowser: disabling JavaScript");
        m_webEngine->page()->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, false);
    }

    m_webEngine->page()->settings()->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);
    m_webEngine->page()->settings()->setAttribute(QWebEngineSettings::AutoLoadIconsForPage, true);
    m_webEngine->page()->settings()->setAttribute(QWebEngineSettings::FocusOnNavigationEnabled, true);
    m_webEngine->page()->settings()->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);
    m_webEngine->page()->settings()->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, true);
    m_webEngine->page()->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);

    if (m_horizontalScrollbar || m_verticalScrollbar)
        m_webEngine->page()->settings()->setAttribute(QWebEngineSettings::ShowScrollBars, false);

    m_webEngine->setWindowFlag(Qt::WindowStaysOnTopHint, true);

    QImage image = QImage(m_actualBrowserArea.size(), QImage::Format_ARGB32);
    m_image = GetPainter()->GetFormatImage();
    m_image->Assign(image);

    SetBackgroundColor(m_bgColor);

    m_zoom = gCoreContext->GetFloatSetting("WebBrowserZoomLevel", 1.0);

    SetZoom(m_zoom);

    if (!m_widgetUrl.isEmpty() && m_widgetUrl.isValid())
        LoadPage(m_widgetUrl);

    m_initialized = true;
}

/** \fn MythUIWebBrowser::~MythUIWebBrowser(void)
 *  \brief the classes destructor
 */
MythUIWebBrowser::~MythUIWebBrowser()
{
    if (m_webEngine)
    {
        m_webEngine->hide();
        m_webEngine->disconnect();
        m_webEngine->deleteLater();
        m_webEngine = nullptr;
    }

    if (m_image)
    {
        m_image->DecrRef();
        m_image = nullptr;
    }
}

/** \fn MythUIWebBrowser::LoadPage(QUrl)
 *  \brief Loads the specified url and displays it.
 *  \param url The url to load
 */
void MythUIWebBrowser::LoadPage(const QUrl& url)
{
    if (!m_webEngine)
        return;

    ResetScrollBars();

    m_webEngine->setUrl(url);
}

/** \fn MythUIWebBrowser::SetHtml(const QString&, const QUrl&)
 *  \brief Sets the content of the widget to the specified html.
 *  \param html the html to display
 *  \param baseUrl External objects referenced in the HTML document are located
 *                 relative to baseUrl.
 */
void MythUIWebBrowser::SetHtml(const QString &html, const QUrl &baseUrl)
{
    if (!m_webEngine)
        return;

    ResetScrollBars();

    m_webEngine->setHtml(html, baseUrl);
}

/** \fn MythUIWebBrowser::LoadUserStyleSheet(QUrl)
 *  \brief Sets the specified user style sheet.
 *  \param url The url to the style sheet
 */
void MythUIWebBrowser::LoadUserStyleSheet(const QUrl& url, const QString &name)
{
    if (!m_webEngine)
        return;

    // download the style sheet
    QByteArray download;
    if (!GetMythDownloadManager()->download(url.toString(), &download))
    {
        LOG(VB_GENERAL, LOG_ERR,"MythUIWebBrowser: Failed to download css from - " + url.toString());
        return;
    }

    RemoveUserStyleSheet("mythtv");

    LOG(VB_GENERAL, LOG_INFO,"MythUIWebBrowser: Loading css from - " + url.toString());

    QWebEngineScript script;
    QString s = QString::fromLatin1("(function() {"\
                                    "    css = document.createElement('style');"\
                                    "    css.type = 'text/css';"\
                                    "    css.id = '%1';"\
                                    "    document.head.appendChild(css);"\
                                    "    css.innerText = '%2';"\
                                    "})()").arg(name, QString(download).simplified());

    m_webEngine->page()->runJavaScript(s, QWebEngineScript::ApplicationWorld);

    script.setName(name);
    script.setSourceCode(s);
    script.setInjectionPoint(QWebEngineScript::DocumentReady);
    script.setRunsOnSubFrames(true);
    script.setWorldId(QWebEngineScript::ApplicationWorld);
    m_webEngine->page()->scripts().insert(script);
}

void MythUIWebBrowser::RemoveUserStyleSheet(const QString &name)
{
    if (!m_webEngine)
        return;

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    QList<QWebEngineScript> scripts = m_webEngine->page()->scripts().find(name);
#else
    QList<QWebEngineScript> scripts = m_webEngine->page()->scripts().findScripts(name);
#endif

    if (!scripts.isEmpty())
    {
        m_webEngine->page()->scripts().remove(scripts[0]);

        QString s = QString::fromLatin1("(function() {"\
                                        "    var element = document.getElementById('%1');"\
                                        "    element.outerHTML = '';"\
                                        "    delete element;"\
                                        "})()").arg(name);

        m_webEngine->page()->runJavaScript(s, QWebEngineScript::ApplicationWorld);
    }
}

QWebEngineSettings *MythUIWebBrowser::GetWebEngineSettings(void)
{
    if (!m_webEngine)
        return nullptr;

    return m_webEngine->settings();
}

QWebEngineProfile *MythUIWebBrowser::GetWebEngineProfile(void)
{
    if (!m_webEngine)
        return nullptr;

    return m_webEngine->page()->profile();
}

void MythUIWebBrowser::SetHttpUserAgent(const QString &userAgent)
{
    if (!m_webEngine)
        return;

    m_webEngine->page()->profile()->setHttpUserAgent(userAgent);
}

/** \fn MythUIWebBrowser::SetBackgroundColor(QColor)
 *  \brief Sets the default background color.
 *  \param color the color to use
 *  \note This will only be used if the HTML document being displayed doesn't
 *        specify a background color to use.
 */
void MythUIWebBrowser::SetBackgroundColor(QColor color)
{
    if (!m_webEngine)
        return;

    color.setAlpha(255);
    QPalette palette = m_webEngine->palette();
    palette.setBrush(QPalette::Window, QBrush(color));
    palette.setBrush(QPalette::Base, QBrush(color));
    m_webEngine->setPalette(palette);

    UpdateBuffer();
}

/** \fn MythUIWebBrowser::SetActive(bool)
 *  \brief Toggles the active state of the widget
 *  \param active the new active state
 *  \note When in an active state the widget will show the mouse pointer,
 *        generate status bar changed signals, allow the active link to be
 *        changed using the keyboard etc.
 *  \warning If you want to show another screen or popup above the screen
 *           owning a MythUIWebBrowser you must first set the active state
 *           to false.
 */
void MythUIWebBrowser::SetActive(bool active)
{
    if (!m_webEngine)
        return;

    if (m_active == active)
        return;

    m_active = active;
    m_wasActive = active;

    if (m_active)
    {
        m_webEngine->setUpdatesEnabled(false);
        m_webEngine->setParent(GetMythMainWindow()->GetPaintWindow());

        if (m_hasFocus)
            m_webEngine->setFocus();

        m_webEngine->show();
        m_webEngine->raise();
        m_webEngine->setUpdatesEnabled(true);
    }
    else
    {
        UpdateBuffer();

        m_webEngine->setParent(GetMythMainWindow());
        m_webEngine->clearFocus();
    }
}

/** \fn MythUIWebBrowser::ZoomIn(void)
 *  \brief Increase the text size
 */
void MythUIWebBrowser::ZoomIn(void)
{
    SetZoom(m_zoom + 0.1);
}

/** \fn MythUIWebBrowser::ZoomOut(void)
 *  \brief Decrease the text size
 */
void MythUIWebBrowser::ZoomOut(void)
{
    SetZoom(m_zoom - 0.1);
}

/** \fn MythUIWebBrowser::SetZoom(double)
 *  \brief Set the text size to specific size
 *  \param zoom The size to use. Useful values are between 0.3 and 5.0
 */
void MythUIWebBrowser::SetZoom(double zoom)
{
    if (!m_webEngine)
        return;

    m_zoom = std::clamp(zoom, 0.3, 5.0);
    m_webEngine->setZoomFactor(m_zoom);

    ResetScrollBars();
    UpdateBuffer();

    slotStatusBarMessage(tr("Zoom: %1%").arg(m_zoom * 100));

    gCoreContext->SaveSetting("WebBrowserZoomLevel", QString("%1").arg(m_zoom));
}

void MythUIWebBrowser::Reload(bool useCache)
{
    if (useCache)
        TriggerPageAction(QWebEnginePage::Reload);
    else
        TriggerPageAction(QWebEnginePage::QWebEnginePage::ReloadAndBypassCache);
}

void MythUIWebBrowser::TriggerPageAction(QWebEnginePage::WebAction action, bool checked)
{
    if (!m_webEngine)
        return;

    m_webEngine->triggerPageAction(action, checked);
}

void MythUIWebBrowser::SetDefaultSaveDirectory(const QString &saveDir)
{
    if (!saveDir.isEmpty())
        m_defaultSaveDir = saveDir;
    else
        m_defaultSaveDir = GetConfDir() + "/MythBrowser/";
}

void MythUIWebBrowser::SetDefaultSaveFilename(const QString &filename)
{
    if (!filename.isEmpty())
        m_defaultSaveFilename = filename;
    else
        m_defaultSaveFilename.clear();
}

/** \fn MythUIWebBrowser::GetZoom()
 *  \brief Get the current zoom level
 *  \return the zoom level
 */
float MythUIWebBrowser::GetZoom(void) const
{
    return m_zoom;
}

/** \fn MythUIWebBrowser::CanGoForward(void)
 *  \brief Can go forward in page history
 *  \return Return true if it is possible to go
 *          forward in the pages visited history
 */
bool MythUIWebBrowser::CanGoForward(void)
{
    if (!m_webEngine)
        return false;

    return m_webEngine->history()->canGoForward();
}

/** \fn MythUIWebBrowser::CanGoBack(void)
 *  \brief Can we go backward in page history
 *  \return Return true if it is possible to go
 *          backward in the pages visited history
 */
bool MythUIWebBrowser::CanGoBack(void)
{
    if (!m_webEngine)
        return false;

    return m_webEngine->history()->canGoBack();
}

/** \fn MythUIWebBrowser::Back(void)
 *  \brief Got backward in page history
 */
void MythUIWebBrowser::Back(void)
{
    if (!m_webEngine)
        return;

    m_webEngine->back();
}

/** \fn MythUIWebBrowser::Forward(void)
 *  \brief Got forward in page history
 */
void MythUIWebBrowser::Forward(void)
{
    if (!m_webEngine)
        return;

    m_webEngine->forward();
}

/** \fn MythUIWebBrowser::GetIcon(void)
 *  \brief Gets the current page's fav icon
 *  \return return the icon if one is available
 */
QIcon MythUIWebBrowser::GetIcon(void)
{
    if (m_webEngine)
        return m_webEngine->icon();

    return {};
}

/** \fn MythUIWebBrowser::GetUrl(void)
 *  \brief Gets the current page's url
 *  \return return the url
 */
QUrl MythUIWebBrowser::GetUrl(void)
{
    if (m_webEngine)
        return m_webEngine->url();

    return {};
}

/** \fn MythUIWebBrowser::GetTitle(void)
 *  \brief Gets the current page's title
 *  \return return the page title
 */
QString MythUIWebBrowser::GetTitle(void)
{
    if (m_webEngine)
        return m_webEngine->title();

    return {""};
}

/** \fn MythUIWebBrowser::runJavaScript(const QString& scriptSource)
 *  \brief Evaluates the JavaScript code in \a scriptSource.
 *  \return void
 */
void MythUIWebBrowser::RunJavaScript(const QString &scriptSource)
{
    if (m_webEngine)
        m_webEngine->page()->runJavaScript(scriptSource);
}

void MythUIWebBrowser::SendStatusBarMessage(const QString &text)
{
    slotStatusBarMessage(text);
}

void MythUIWebBrowser::slotTakingFocus(void)
{
    SetActive(true);
    m_webEngine->setFocus();
}

void MythUIWebBrowser::slotLosingFocus(void)
{
     GetMythMainWindow()->GetPaintWindow()->setFocus();
     m_webEngine->clearFocus();
}

void MythUIWebBrowser::slotRenderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus terminationStatus, int exitCode)
{
    LOG(VB_GENERAL, LOG_INFO, QString("MythUIWebBrowser::slotRenderProcessTerminated - terminationStatus: %1, exitStatus: %2").arg(terminationStatus).arg(exitCode));
    m_webEngine->reload();
}

void MythUIWebBrowser::slotFullScreenRequested(QWebEngineFullScreenRequest fullScreenRequest)
{
    fullScreenRequest.accept();
}

void MythUIWebBrowser::slotWindowCloseRequested(void)
{
    LOG(VB_GENERAL, LOG_INFO, QString("MythUIWebBrowser::slotWindowCloseRequested called"));

    Back();
}

void MythUIWebBrowser::MythUIWebBrowser::slotLoadStarted(void)
{
    ResetScrollBars();

    emit loadStarted();
}

void MythUIWebBrowser::slotLoadFinished(bool ok)
{
    UpdateBuffer();
    emit loadFinished(ok);
}

void MythUIWebBrowser::slotLoadProgress(int progress)
{
    emit loadProgress(progress);
}

void MythUIWebBrowser::slotTitleChanged(const QString &title)
{
    emit titleChanged(title);
}

void MythUIWebBrowser::slotStatusBarMessage(const QString &text)
{
    emit statusBarMessage(text);
}

void MythUIWebBrowser::slotLinkClicked(const QUrl &url)
{
    LoadPage(url);
}

void MythUIWebBrowser::slotIconChanged(const QIcon &icon)
{
    emit iconChanged(icon);
}

void MythUIWebBrowser::slotIconUrlChanged(const QUrl &url)
{
    emit iconUrlChanged(url);
}

void MythUIWebBrowser::slotScrollPositionChanged(const QPointF  /*position*/)
{
    UpdateScrollBars();
}

void MythUIWebBrowser::slotContentsSizeChanged(const QSizeF /*size*/)
{
    UpdateScrollBars();
}

void MythUIWebBrowser::slotScrollBarShowing(void)
{
//    bool wasActive = (m_wasActive || m_active);
//    SetActive(false);
//    m_wasActive = wasActive;
}

void MythUIWebBrowser::slotScrollBarHiding(void)
{
//    SetActive(m_wasActive);
//    slotTopScreenChanged(nullptr);
}

void MythUIWebBrowser::slotTopScreenChanged(MythScreenType * /* screen */)
{
    UpdateBuffer();

    if (IsOnTopScreen())
        SetActive(m_wasActive);
    else
    {
        bool wasActive = (m_wasActive || m_active);
        SetActive(false);
        m_wasActive = wasActive;
    }
}

/// is our containing screen the top screen?
bool MythUIWebBrowser::IsOnTopScreen(void)
{
     if (!m_parentScreen)
        return false;

    for (int x = GetMythMainWindow()->GetStackCount() - 1; x >= 0; x--)
    {
        MythScreenStack *stack = GetMythMainWindow()->GetStackAt(x);

        // ignore stacks with no screens on them
        if (!stack->GetTopScreen())
            continue;

        return (stack->GetTopScreen() == m_parentScreen);
    }

    return false;
}


void MythUIWebBrowser::UpdateScrollBars(void)
{
    if (!m_webEngine)
        return;

    QPoint position = m_webEngine->page()->scrollPosition().toPoint();
    if (m_verticalScrollbar)
    {
        int maximum = m_webEngine->page()->contentsSize().height() - m_actualBrowserArea.height();
        m_verticalScrollbar->SetMaximum(maximum);
        m_verticalScrollbar->SetPageStep(m_actualBrowserArea.height());
        m_verticalScrollbar->SetSliderPosition(position.y());
    }

    if (m_horizontalScrollbar)
    {
        int maximum = m_webEngine->page()->contentsSize().width() - m_actualBrowserArea.width();
        m_horizontalScrollbar->SetMaximum(maximum);
        m_horizontalScrollbar->SetPageStep(m_actualBrowserArea.width());
        m_horizontalScrollbar->SetSliderPosition(position.x());
    }
}

void MythUIWebBrowser::UpdateBuffer(void)
{
    UpdateScrollBars();

    if (!m_image || !m_webEngine)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("MythUIWebBrowser::UpdateBuffer called - m_image or m_webEngine is FALSE"));
        return;
    }

    if (!m_active || (m_active && m_webEngine->hasFocus()))
    {
        m_webEngine->render(m_image);
        m_image->SetChanged();
        Refresh();
    }
}

/**
 *  \copydoc MythUIType::Pulse()
 */
void MythUIWebBrowser::Pulse(void)
{
    //SetRedraw();

    if (!m_webEngine)
        return;

    if (m_updateInterval && m_lastUpdateTime.hasExpired(m_updateInterval))
    {
        UpdateBuffer();
        m_lastUpdateTime.start();
    }

    MythUIType::Pulse();
}

/**
 *  \copydoc MythUIType::DrawSelf()
 */
void MythUIWebBrowser::DrawSelf(MythPainter *p, int xoffset, int yoffset,
                                int alphaMod, QRect clipRect)
{
    if (!m_image || m_image->isNull() || !m_webEngine || m_webEngine->hasFocus())
        return;

    QRect area = m_actualBrowserArea;
    area.translate(xoffset, yoffset);

    p->SetClipRect(clipRect);
    p->DrawImage(area.x(), area.y(), m_image, alphaMod);
}

void MythUIWebBrowser::HandleMouseAction(const QString &action)
{
    int step = 5;

    // speed up mouse movement if the same key is held down
    if (action == m_lastMouseAction &&
        m_lastMouseActionTime.isValid() &&
        !m_lastMouseActionTime.hasExpired(500))
    {
        m_lastMouseActionTime.start();
        m_mouseKeyCount++;

        if (m_mouseKeyCount > 5)
            step = 25;
    }
    else
    {
        m_lastMouseAction = action;
        m_lastMouseActionTime.start();
        m_mouseKeyCount = 1;
    }

    if (action == "MOUSEUP")
    {
        QPoint curPos = QCursor::pos();
        QCursor::setPos(curPos.x(), curPos.y() - step);
    }
    else if (action == "MOUSELEFT")
    {
        QPoint curPos = QCursor::pos();
        QCursor::setPos(curPos.x() - step, curPos.y());
    }
    else if (action == "MOUSERIGHT")
    {
        QPoint curPos = QCursor::pos();
        QCursor::setPos(curPos.x() + step, curPos.y());
    }
    else if (action == "MOUSEDOWN")
    {
        QPoint curPos = QCursor::pos();
        QCursor::setPos(curPos.x(), curPos.y() + step);
    }
    else if (action == "MOUSELEFTBUTTON")
    {
        QPoint curPos = QCursor::pos();
        QWidget *widget = QApplication::widgetAt(curPos);

        if (widget)
        {
            curPos = widget->mapFromGlobal(curPos);

            auto *me = new QMouseEvent(QEvent::MouseButtonPress, curPos, curPos,
                                              Qt::LeftButton, Qt::LeftButton,
                                              Qt::NoModifier);
            //FIXME
            QCoreApplication::postEvent(widget, me);

            me = new QMouseEvent(QEvent::MouseButtonRelease, curPos, curPos,
                                 Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            //FIXME
            QCoreApplication::postEvent(widget, me);
        }
    }
}

void MythUIWebBrowser::ResetScrollBars()
{
    if (m_verticalScrollbar)
    {
        m_verticalScrollbar->Reset();
        m_verticalScrollbar->Hide();
    }

    if (m_horizontalScrollbar)
    {
        m_horizontalScrollbar->Reset();
        m_horizontalScrollbar->Hide();
    }
}

/**
 *  \copydoc MythUIType::ParseElement()
 */
bool MythUIWebBrowser::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    if (element.tagName() == "zoom")
    {
        QString zoom = getFirstText(element);
        m_zoom = zoom.toDouble();
    }
    else if (element.tagName() == "url")
    {
        m_widgetUrl.setUrl(getFirstText(element));
    }
    else if (element.tagName() == "userstylesheet")
    {
        m_userCssFile = getFirstText(element);
    }
    else if (element.tagName() == "updateinterval")
    {
        QString interval = getFirstText(element);
        m_updateInterval = interval.toInt();
    }
    else if (element.tagName() == "background")
    {
        m_bgColor = QColor(element.attribute("color", "#ffffff"));
        int alpha = element.attribute("alpha", "255").toInt();
        m_bgColor.setAlpha(alpha);
    }
    else if (element.tagName() == "browserarea")
    {
        m_browserArea = parseRect(element);
    }
    else if (element.tagName() == "scrollduration")
    {
        // this is no longer used QWebEngine has its own scroll animation
    }
    else if (element.tagName() == "acceptsfocus")
    {
        SetCanTakeFocus(parseBool(element));
    }
    else
    {
        return MythUIType::ParseElement(filename, element, showWarnings);
    }

    return true;
}

/**
 *  \copydoc MythUIType::CopyFrom()
 */
void MythUIWebBrowser::CopyFrom(MythUIType *base)
{
    auto *browser = dynamic_cast<MythUIWebBrowser *>(base);
    if (!browser)
    {
        LOG(VB_GENERAL, LOG_ERR, "ERROR, bad parsing");
        return;
    }

    MythUIType::CopyFrom(base);

    m_browserArea = browser->m_browserArea;
    m_zoom = browser->m_zoom;
    m_bgColor = browser->m_bgColor;
    m_widgetUrl = browser->m_widgetUrl;
    m_userCssFile = browser->m_userCssFile;
    m_updateInterval = browser->m_updateInterval;
    m_defaultSaveDir = browser->m_defaultSaveDir;
    m_defaultSaveFilename = browser->m_defaultSaveFilename;
    //m_scrollAnimation.setDuration(browser->m_scrollAnimation.duration());
}

/**
 *  \copydoc MythUIType::CreateCopy()
 */
void MythUIWebBrowser::CreateCopy(MythUIType *parent)
{
    auto *browser = new MythUIWebBrowser(parent, objectName());
    browser->CopyFrom(this);
}

#include "moc_mythuiwebbrowser.cpp"
