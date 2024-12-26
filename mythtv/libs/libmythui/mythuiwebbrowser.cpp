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
#include <QWebFrame>
#include <QWebHistory>
#include <QPainter>
#include <QDir>
#include <QBuffer>
#include <QStyle>
#include <QKeyEvent>
#include <QDomDocument>
#include <QNetworkCookieJar>
#include <QNetworkConfiguration>

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

QNetworkReply* MythNetworkAccessManager::createRequest(Operation op, const QNetworkRequest& req, QIODevice* outgoingData)
{
    QNetworkReply* reply = QNetworkAccessManager::createRequest(op, req, outgoingData);
    reply->ignoreSslErrors();
    return reply;
}

static MythNetworkAccessManager *networkManager = nullptr;

static void DestroyNetworkAccessManager(void)
{
    if (networkManager)
    {
        delete networkManager;
        networkManager = nullptr;
    }
}

static QNetworkAccessManager *GetNetworkAccessManager(void)
{
    if (networkManager)
        return networkManager;

    networkManager = new MythNetworkAccessManager();
//  This next line prevents seg fault at program exit in
//  QNetworkConfiguration::~QNetworkConfiguration()
//  when destructor is called by DestroyNetworkAccessManager
    networkManager->setConfiguration(networkManager->configuration());
    LOG(VB_GENERAL, LOG_DEBUG, "Copying DLManager's Cookie Jar");
    GetMythDownloadManager()->loadCookieJar(GetConfDir() + "/MythBrowser/cookiejar.txt");
    networkManager->setCookieJar(GetMythDownloadManager()->copyCookieJar());

    atexit(DestroyNetworkAccessManager);

    return networkManager;
}

/**
 * @class BrowserApi
 * @brief Adds a JavaScript object
 * \note allows the browser to control the music player
 */
BrowserApi::BrowserApi(QObject *parent)
           : QObject(parent)
{
    gCoreContext->addListener(this);
}

BrowserApi::~BrowserApi(void)
{
    gCoreContext->removeListener(this);
}

void BrowserApi::setWebView(QWebView *view)
{
    QWebPage *page = view->page();
    m_frame = page->mainFrame();

    attachObject();
    connect(m_frame, &QWebFrame::javaScriptWindowObjectCleared, this,
            &BrowserApi::attachObject);
}

void BrowserApi::attachObject(void)
{
    m_frame->addToJavaScriptWindowObject(QString("MusicPlayer"), this);
}

void BrowserApi::Play(void)
{
    MythEvent me(QString("MUSIC_COMMAND %1 PLAY").arg(gCoreContext->GetHostName()));
    gCoreContext->dispatch(me);
}

void BrowserApi::Stop(void)
{
    MythEvent me(QString("MUSIC_COMMAND %1 STOP").arg(gCoreContext->GetHostName()));
    gCoreContext->dispatch(me);
}

void BrowserApi::Pause(void)
{
    MythEvent me(QString("MUSIC_COMMAND %1 PAUSE %1").arg(gCoreContext->GetHostName()));
    gCoreContext->dispatch(me);
}

void BrowserApi::SetVolume(int volumn)
{
    MythEvent me(QString("MUSIC_COMMAND %1 SET_VOLUME %2")
                 .arg(gCoreContext->GetHostName()).arg(volumn));
    gCoreContext->dispatch(me);
}

int BrowserApi::GetVolume(void)
{
    m_gotAnswer = false;

    MythEvent me(QString("MUSIC_COMMAND %1 GET_VOLUME")
                 .arg(gCoreContext->GetHostName()));
    gCoreContext->dispatch(me);

    QElapsedTimer timer;
    timer.start();

    while (!timer.hasExpired(2000) && !m_gotAnswer)
    {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(10ms);
    }

    if (m_gotAnswer)
        return m_answer.toInt();

    return -1;
}

void BrowserApi::PlayFile(const QString& filename)
{
    MythEvent me(QString("MUSIC_COMMAND %1 PLAY_FILE '%2'")
                 .arg(gCoreContext->GetHostName(), filename));
    gCoreContext->dispatch(me);
}

void BrowserApi::PlayTrack(int trackID)
{
    MythEvent me(QString("MUSIC_COMMAND %1 PLAY_TRACK %2")
                 .arg(gCoreContext->GetHostName()).arg(trackID));
    gCoreContext->dispatch(me);
}

void BrowserApi::PlayURL(const QString& url)
{
    MythEvent me(QString("MUSIC_COMMAND %1 PLAY_URL %2")
                 .arg(gCoreContext->GetHostName(), url));
    gCoreContext->dispatch(me);
}

QString BrowserApi::GetMetadata(void)
{
    m_gotAnswer = false;

    MythEvent me(QString("MUSIC_COMMAND %1 GET_METADATA")
                 .arg(gCoreContext->GetHostName()));
    gCoreContext->dispatch(me);

    QElapsedTimer timer;
    timer.start();

    while (!timer.hasExpired(2000)  && !m_gotAnswer)
    {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(10ms);
    }

    if (m_gotAnswer)
        return m_answer;

    return {"unknown"};
}

void BrowserApi::customEvent(QEvent *e)
{
    if (e->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(e);
        if (me == nullptr)
            return;

        const QString& message = me->Message();

        if (!message.startsWith("MUSIC_CONTROL"))
            return;

        QStringList tokens = message.simplified().split(" ");

        if ((tokens.size() >= 4) && (tokens[1] == "ANSWER")
            && (tokens[2] == gCoreContext->GetHostName()))
        {
            m_answer = tokens[3];

            for (int i = 4; i < tokens.size(); i++)
                m_answer += QString(" ") + tokens[i];

            m_gotAnswer = true;
        }
    }
}

MythWebPage::MythWebPage(QObject *parent)
            : QWebPage(parent)
{
    setNetworkAccessManager(GetNetworkAccessManager());
}

MythWebPage::~MythWebPage()
{
    LOG(VB_GENERAL, LOG_DEBUG, "Refreshing DLManager's Cookie Jar");
    GetMythDownloadManager()->refreshCookieJar(networkManager->cookieJar());
    GetMythDownloadManager()->saveCookieJar(GetConfDir() + "/MythBrowser/cookiejar.txt");
}

bool MythWebPage::supportsExtension(Extension extension) const
{
    return extension == QWebPage::ErrorPageExtension;
}

bool MythWebPage::extension(Extension extension, const ExtensionOption *option,
                            ExtensionReturn *output)
{
    if (extension == QWebPage::ErrorPageExtension)
    {
        if (!option || !output)
            return false;

        // Using static_cast yields the clang-tidy warning: do not use
        // static_cast to downcast from a base to a derived class; use
        // dynamic_cast instead.  Using dynamic-cast yields the
        // compiler error: 'QWebPage::ExtensionOption' is not
        // polymorphic.
        //
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
        const auto *erroroption = static_cast<const ErrorPageExtensionOption *>(option);
        ErrorPageExtensionReturn *erroroutput = nullptr;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
        erroroutput = static_cast<ErrorPageExtensionReturn *>(output);

        QString filename = "htmls/notfound.html";

        if (!GetMythUI()->FindThemeFile(filename))
            return false;

        QFile file(QLatin1String(qPrintable(filename)));
        bool isOpened = file.open(QIODevice::ReadOnly);

        if (!isOpened)
            return false;

        QString title = tr("Error loading page: %1").arg(erroroption->url.toString());
        QString html = QString(QLatin1String(file.readAll()))
                       .arg(title,
                            erroroption->errorString,
                            erroroption->url.toString());

        QBuffer imageBuffer;
        imageBuffer.open(QBuffer::ReadWrite);
        QIcon icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning,
                                                 nullptr, nullptr);
        QPixmap pixmap = icon.pixmap(QSize(32, 32));

        if (pixmap.save(&imageBuffer, "PNG"))
        {
            html.replace(QLatin1String("IMAGE_BINARY_DATA_HERE"),
                         QString(QLatin1String(imageBuffer.buffer().toBase64())));
        }

        erroroutput->content = html.toUtf8();

        return true;
    }

    return false;
}

QString MythWebPage::userAgentForUrl(const QUrl &url) const
{
    return QWebPage::userAgentForUrl(url).replace("Safari", "MythBrowser");
}

/**
 * @class MythWebView
 * @brief Subclass of QWebView
 * \note allows us to intercept keypresses
 */
MythWebView::MythWebView(QWidget *parent, MythUIWebBrowser *parentBrowser)
            : QWebView(parent),
      m_webpage(new MythWebPage(this)),
      m_parentBrowser(parentBrowser),
      m_api(new BrowserApi(this))
{
    setPage(m_webpage);

    connect(page(), &QWebPage::unsupportedContent,
            this, &MythWebView::handleUnsupportedContent);

    connect(page(), &QWebPage::downloadRequested,
            this, &MythWebView::handleDownloadRequested);

    page()->setForwardUnsupportedContent(true);

    m_api->setWebView(this);
}

MythWebView::~MythWebView(void)
{
    delete m_webpage;
    delete m_api;
}

/**
 *  \copydoc MythUIType::keyPressEvent()
 */

const QString kgetType { QStringLiteral("\
function activeElement()\
{\
    var type;\
    type = document.activeElement.type;\
    return type;\
}\
activeElement();") };

void MythWebView::keyPressEvent(QKeyEvent *event)
{
    // does an edit have focus?
    QString type = m_parentBrowser->evaluateJavaScript(kgetType)
                                                    .toString().toLower();
    bool editHasFocus = (type == "text" || type == "textarea" ||
                         type == "password");

    // if the QWebView widget has focus then all keypresses from a regular
    // keyboard get sent here first
    if (editHasFocus || m_parentBrowser->IsInputToggled())
    {
        // input is toggled so pass all keypresses to the QWebView's handler
        QWebView::keyPressEvent(event);
    }
    else
    {
        // we need to convert a few keypress events so the QWebView does the
        // right thing
        QStringList actions;
        bool handled = false;
        handled = GetMythMainWindow()->TranslateKeyPress("Browser", event,
                                                         actions);

        for (int i = 0; i < actions.size() && !handled; i++)
        {
            const QString& action = actions[i];
            handled = true;

            if (action == "NEXTLINK")
            {
                QKeyEvent tabKey(event->type(), Qt::Key_Tab,
                                 event->modifiers(), QString(),
                                 event->isAutoRepeat(), event->count());
                *event = tabKey;
                QWebView::keyPressEvent(event);
                return;
            }
            if (action == "PREVIOUSLINK")
            {
                QKeyEvent shiftTabKey(event->type(), Qt::Key_Tab,
                                      event->modifiers() | Qt::ShiftModifier,
                                      QString(),
                                      event->isAutoRepeat(), event->count());
                *event = shiftTabKey;
                QWebView::keyPressEvent(event);
                return;
            }
            if (action == "FOLLOWLINK")
            {
                QKeyEvent returnKey(event->type(), Qt::Key_Return,
                                    event->modifiers(), QString(),
                                    event->isAutoRepeat(), event->count());
                *event = returnKey;
                QWebView::keyPressEvent(event);
                return;
            }
        }

        // pass the keyPress event to our main window handler so they get
        // handled properly by the various mythui handlers
        QCoreApplication::postEvent(GetMythMainWindow(), new QKeyEvent(*event));
    }
}

void MythWebView::handleUnsupportedContent(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError)
    {
        stop();

        QVariant header = reply->header(QNetworkRequest::ContentTypeHeader);

        if (header != QVariant())
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("MythWebView::handleUnsupportedContent - %1")
                .arg(header.toString()));
        }

        m_downloadReply = reply;
        m_downloadRequest = reply->request();
        m_downloadAndPlay = false;
        showDownloadMenu();

        return;
    }
}

void  MythWebView::handleDownloadRequested(const QNetworkRequest &request)
{
    m_downloadReply = nullptr;
    doDownloadRequested(request);
}

void  MythWebView::doDownloadRequested(const QNetworkRequest &request)
{
    m_downloadRequest = request;

    // get the filename from the url if available
    QFileInfo fi(request.url().path());
    QString basename(fi.completeBaseName());
    QString extension = fi.suffix().toLower();
    QString mimetype = getReplyMimetype();

    // if we have a default filename use that
    QString saveBaseName = basename;

    if (!m_parentBrowser->GetDefaultSaveFilename().isEmpty())
    {
        QFileInfo savefi(m_parentBrowser->GetDefaultSaveFilename());
        saveBaseName = savefi.completeBaseName();
    }

    // if the filename is still empty use a default name
    if (saveBaseName.isEmpty())
        saveBaseName = "unnamed_download";

    // if we don't have an extension from the filename get one from the mime type
    if (extension.isEmpty())
        extension = getExtensionForMimetype(mimetype);

    if (!extension.isEmpty())
        extension = '.' + extension;

    QString saveFilename = QString("%1%2%3")
                                .arg(m_parentBrowser->GetDefaultSaveDirectory(),
                                     saveBaseName,
                                     extension);

    // dont overwrite an existing file
    if (QFile::exists(saveFilename))
    {
        int i = 1;

        do
        {
            saveFilename = QString("%1%2-%3%4")
                                .arg(m_parentBrowser->GetDefaultSaveDirectory(),
                                     saveBaseName,
                                     QString::number(i++),
                                     extension);
        }
        while (QFile::exists(saveFilename));
    }

    // if we are downloading and then playing the file don't ask for the file name
    if (m_downloadAndPlay)
    {
        doDownload(saveFilename);
    }
    else
    {
        MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

        QString msg = tr("Enter filename to save file");
        auto *input = new MythTextInputDialog(popupStack, msg, FilterNone,
                                              false, saveFilename);

        if (input->Create())
        {
            input->SetReturnEvent(this, "filenamedialog");
            popupStack->AddScreen(input);
        }
        else
        {
            delete input;
        }
    }
}

void MythWebView::doDownload(const QString &saveFilename)
{
    if (saveFilename.isEmpty())
        return;

    openBusyPopup(tr("Downloading file. Please wait..."));

    // No need to make sure the path to saveFilename exists because
    // MythDownloadManage takes care of that
    GetMythDownloadManager()->queueDownload(m_downloadRequest.url().toString(),
                                            saveFilename, this);
}

void MythWebView::openBusyPopup(const QString &message)
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

void MythWebView::closeBusyPopup(void)
{
    if (m_busyPopup)
        m_busyPopup->Close();

    m_busyPopup = nullptr;
}

void MythWebView::customEvent(QEvent *event)
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
            doDownload(resulttext);
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
                        QString("MythWebView: Asked to play a file with "
                                "extension '%1' but don't know how")
                        .arg(extension));
                }
            }
            else if (resulttext == tr("Download the file"))
            {
                doDownloadRequested(m_downloadRequest);
            }
            else if (resulttext == tr("Download and play the file"))
            {
                m_downloadAndPlay = true;
                doDownloadRequested(m_downloadRequest);
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
}

void MythWebView::showDownloadMenu(void)
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

QString MythWebView::getExtensionForMimetype(const QString &mimetype)
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

bool MythWebView::isMusicFile(const QString &extension, const QString &mimetype)
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

bool MythWebView::isVideoFile(const QString &extension, const QString &mimetype)
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

QString MythWebView::getReplyMimetype(void)
{
    if (!m_downloadReply)
        return {};

    QString mimeType;
    QVariant header = m_downloadReply->header(QNetworkRequest::ContentTypeHeader);

    if (header != QVariant())
        mimeType = header.toString();

    return mimeType;
}

QWebView *MythWebView::createWindow(QWebPage::WebWindowType /* type */)
{
    return (QWebView *) this;
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
      m_bgColor("White"),    m_userCssFile(""),
      m_defaultSaveDir(GetConfDir() + "/MythBrowser/"),
      m_defaultSaveFilename(""),
      m_lastMouseAction("")
{
    SetCanTakeFocus(true);
    m_scrollAnimation.setDuration(0);
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

    m_browser = new MythWebView(GetMythMainWindow()->GetPaintWindow(), this);
    m_browser->setPalette(QApplication::style()->standardPalette());
    m_browser->setGeometry(m_actualBrowserArea);
    m_browser->setFixedSize(m_actualBrowserArea.size());
    m_browser->move(m_actualBrowserArea.x(), m_actualBrowserArea.y());
    m_browser->page()->setLinkDelegationPolicy(QWebPage::DontDelegateLinks);

    bool err = false;
    UIUtilW::Assign(this, m_horizontalScrollbar, "horizscrollbar", &err);
    UIUtilW::Assign(this, m_verticalScrollbar, "vertscrollbar", &err);
    if (m_horizontalScrollbar)
    {
        QWebFrame* frame = m_browser->page()->currentFrame();
        frame->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);
        connect(m_horizontalScrollbar, &MythUIType::Hiding,
                this, &MythUIWebBrowser::slotScrollBarHiding);
        connect(m_horizontalScrollbar, &MythUIType::Showing,
                this, &MythUIWebBrowser::slotScrollBarShowing);
    }

    if (m_verticalScrollbar)
    {
        QWebFrame* frame = m_browser->page()->currentFrame();
        frame->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);
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

    m_browser->winId();

    SetActive(m_active);

    connect(m_browser, &QWebView::loadStarted,
            this, &MythUIWebBrowser::slotLoadStarted);
    connect(m_browser, &QWebView::loadFinished,
            this, &MythUIWebBrowser::slotLoadFinished);
    connect(m_browser, &QWebView::loadProgress,
            this, &MythUIWebBrowser::slotLoadProgress);
    connect(m_browser, &QWebView::titleChanged,
            this, &MythUIWebBrowser::slotTitleChanged);
    connect(m_browser, &QWebView::iconChanged,
            this, &MythUIWebBrowser::slotIconChanged);
    connect(m_browser, &QWebView::statusBarMessage,
            this, &MythUIWebBrowser::slotStatusBarMessage);
    connect(m_browser->page(), &QWebPage::linkHovered,
            this, &MythUIWebBrowser::slotStatusBarMessage);
    connect(m_browser, &QWebView::linkClicked,
            this, &MythUIWebBrowser::slotLinkClicked);

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
        LOG(VB_GENERAL, LOG_ERR,
            "MythUIWebBrowser: failed to find our parent screen");

    // connect to the topScreenChanged signals on each screen stack
    for (int x = 0; x < GetMythMainWindow()->GetStackCount(); x++)
    {
        MythScreenStack *stack = GetMythMainWindow()->GetStackAt(x);

        if (stack)
            connect(stack, &MythScreenStack::topScreenChanged,
                    this, &MythUIWebBrowser::slotTopScreenChanged);
    }

    // set up the icon cache directory
    QString path = GetConfDir();
    QDir dir(path);

    if (!dir.exists())
        dir.mkdir(path);

    path += "/MythBrowser";
    dir.setPath(path);

    if (!dir.exists())
        dir.mkdir(path);

    QWebSettings::setIconDatabasePath(path);

    if (gCoreContext->GetNumSetting("WebBrowserEnablePlugins", 1) == 1)
    {
        LOG(VB_GENERAL, LOG_INFO, "MythUIWebBrowser: enabling plugins");
        QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled,
                                                     true);
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, "MythUIWebBrowser: disabling plugins");
        QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled,
                                                     false);
    }

    if (!gCoreContext->GetBoolSetting("WebBrowserEnableJavascript",true))
    {
        LOG(VB_GENERAL, LOG_INFO, "MythUIWebBrowser: disabling JavaScript");
        QWebSettings::globalSettings()->setAttribute(QWebSettings::JavascriptEnabled, false);
    }

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
    if (m_browser)
    {
        m_browser->hide();
        m_browser->disconnect();
        m_browser->deleteLater();
        m_browser = nullptr;
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
    if (!m_browser)
        return;

    ResetScrollBars();

    m_browser->setUrl(url);
}

/** \fn MythUIWebBrowser::SetHtml(const QString&, const QUrl&)
 *  \brief Sets the content of the widget to the specified html.
 *  \param html the html to display
 *  \param baseUrl External objects referenced in the HTML document are located
 *                 relative to baseUrl.
 */
void MythUIWebBrowser::SetHtml(const QString &html, const QUrl &baseUrl)
{
    if (!m_browser)
        return;

    ResetScrollBars();

    m_browser->setHtml(html, baseUrl);
}

/** \fn MythUIWebBrowser::LoadUserStyleSheet(QUrl)
 *  \brief Sets the specified user style sheet.
 *  \param url The url to the style sheet
 */
void MythUIWebBrowser::LoadUserStyleSheet(const QUrl& url)
{
    if (!m_browser)
        return;

    LOG(VB_GENERAL, LOG_INFO,
        "MythUIWebBrowser: Loading css from - " + url.toString());

    m_browser->page()->settings()->setUserStyleSheetUrl(url);
}

/** \fn MythUIWebBrowser::SetBackgroundColor(QColor)
 *  \brief Sets the default background color.
 *  \param color the color to use
 *  \note This will only be used if the HTML document being displayed doesn't
 *        specify a background color to use.
 */
void MythUIWebBrowser::SetBackgroundColor(QColor color)
{
    if (!m_browser)
        return;

    color.setAlpha(255);
    QPalette palette = m_browser->page()->palette();
    palette.setBrush(QPalette::Window, QBrush(color));
    palette.setBrush(QPalette::Base, QBrush(color));
    m_browser->page()->setPalette(palette);

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
    if (!m_browser)
        return;

    if (m_active == active)
        return;

    m_active = active;
    m_wasActive = active;

    if (m_active)
    {
        m_browser->setUpdatesEnabled(false);
        m_browser->setFocus();
        m_browser->show();
        m_browser->raise();
        if (QGuiApplication::platformName().contains("egl"))
        {
            m_browser->setParent(nullptr);
            m_browser->setFocus();
            m_browser->show();
            m_browser->raise();
        }
        m_browser->setUpdatesEnabled(true);
    }
    else
    {
        m_browser->clearFocus();
        m_browser->hide();
        if (QGuiApplication::platformName().contains("egl"))
            m_browser->setParent(GetMythMainWindow());
        UpdateBuffer();
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
    if (!m_browser)
        return;

    m_zoom = std::clamp(zoom, 0.3, 5.0);
    m_browser->setZoomFactor(m_zoom);
    ResetScrollBars();
    UpdateBuffer();

    slotStatusBarMessage(tr("Zoom: %1%").arg(m_zoom * 100));

    gCoreContext->SaveSetting("WebBrowserZoomLevel", QString("%1").arg(m_zoom));
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
    if (!m_browser)
        return false;

    return m_browser->history()->canGoForward();
}

/** \fn MythUIWebBrowser::CanGoBack(void)
 *  \brief Can we go backward in page history
 *  \return Return true if it is possible to go
 *          backward in the pages visited history
 */
bool MythUIWebBrowser::CanGoBack(void)
{
    if (!m_browser)
        return false;

    return m_browser->history()->canGoBack();
}

/** \fn MythUIWebBrowser::Back(void)
 *  \brief Got backward in page history
 */
void MythUIWebBrowser::Back(void)
{
    if (!m_browser)
        return;

    m_browser->back();
}

/** \fn MythUIWebBrowser::Forward(void)
 *  \brief Got forward in page history
 */
void MythUIWebBrowser::Forward(void)
{
    if (!m_browser)
        return;

    m_browser->forward();
}

/** \fn MythUIWebBrowser::GetIcon(void)
 *  \brief Gets the current page's fav icon
 *  \return return the icon if one is available
 */
QIcon MythUIWebBrowser::GetIcon(void)
{
    if (m_browser)
    {
        return QWebSettings::iconForUrl(m_browser->url());
    }
    return {};
}

/** \fn MythUIWebBrowser::GetUrl(void)
 *  \brief Gets the current page's url
 *  \return return the url
 */
QUrl MythUIWebBrowser::GetUrl(void)
{
    if (m_browser)
    {
        return m_browser->url();
    }
    return {};
}

/** \fn MythUIWebBrowser::GetTitle(void)
 *  \brief Gets the current page's title
 *  \return return the page title
 */
QString MythUIWebBrowser::GetTitle(void)
{
    if (m_browser)
        return m_browser->title();
    return {""};
}

/** \fn MythUIWebBrowser::evaluateJavaScript(const QString& scriptSource)
 *  \brief Evaluates the JavaScript code in \a scriptSource.
 *  \return QVariant
 */
QVariant MythUIWebBrowser::evaluateJavaScript(const QString &scriptSource)
{
    if (m_browser)
    {
        QWebFrame *frame = m_browser->page()->currentFrame();
        return frame->evaluateJavaScript(scriptSource);
    }
    return {};
}

void MythUIWebBrowser::Scroll(int dx, int dy)
{
    if (!m_browser)
        return;

    QPoint startPos = m_browser->page()->currentFrame()->scrollPosition();
    QPoint endPos = startPos + QPoint(dx, dy);

    if (GetPainter()->SupportsAnimation() && m_scrollAnimation.duration() > 0)
    {
        // Previous scroll has been completed
        if (m_destinationScrollPos == startPos)
            m_scrollAnimation.setEasingCurve(QEasingCurve::InOutCubic);
        else
            m_scrollAnimation.setEasingCurve(QEasingCurve::OutCubic);

        m_destinationScrollPos = endPos;
        m_scrollAnimation.setStartValue(startPos);
        m_scrollAnimation.setEndValue(m_destinationScrollPos);
        m_scrollAnimation.Activate();
    }
    else
    {
        m_destinationScrollPos = endPos;
        m_browser->page()->currentFrame()->setScrollPosition(endPos);
        UpdateBuffer();
    }
}

void MythUIWebBrowser::slotLoadStarted(void)
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

void MythUIWebBrowser::slotIconChanged(void)
{
    emit iconChanged();
}

void MythUIWebBrowser::slotScrollBarShowing(void)
{
    bool wasActive = (m_wasActive || m_active);
    SetActive(false);
    m_wasActive = wasActive;
}

void MythUIWebBrowser::slotScrollBarHiding(void)
{
    SetActive(m_wasActive);
    slotTopScreenChanged(nullptr);
}

void MythUIWebBrowser::slotTopScreenChanged(MythScreenType * /* screen */)
{
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
    if (!m_browser)
        return;

    QPoint position = m_browser->page()->currentFrame()->scrollPosition();
    if (m_verticalScrollbar)
    {
        int maximum =
            m_browser->page()->currentFrame()->contentsSize().height() -
            m_actualBrowserArea.height();
        m_verticalScrollbar->SetMaximum(maximum);
        m_verticalScrollbar->SetPageStep(m_actualBrowserArea.height());
        m_verticalScrollbar->SetSliderPosition(position.y());
    }

    if (m_horizontalScrollbar)
    {
        int maximum =
            m_browser->page()->currentFrame()->contentsSize().width() -
            m_actualBrowserArea.width();
        m_horizontalScrollbar->SetMaximum(maximum);
        m_horizontalScrollbar->SetPageStep(m_actualBrowserArea.width());
        m_horizontalScrollbar->SetSliderPosition(position.x());
    }
}

void MythUIWebBrowser::UpdateBuffer(void)
{
    UpdateScrollBars();

    if (!m_image || !m_browser)
        return;

    if (!m_active || (m_active && !m_browser->hasFocus()))
    {
        QPainter painter(m_image);
        m_browser->render(&painter);
        painter.end();

        m_image->SetChanged();
        Refresh();
    }
}

/**
 *  \copydoc MythUIType::Pulse()
 */
void MythUIWebBrowser::Pulse(void)
{
    if (!m_browser)
        return;

    if (m_scrollAnimation.IsActive() &&
        m_destinationScrollPos !=
        m_browser->page()->currentFrame()->scrollPosition())
    {
        m_scrollAnimation.IncrementCurrentTime();

        QPoint scrollPosition = m_scrollAnimation.currentValue().toPoint();
        m_browser->page()->currentFrame()->setScrollPosition(scrollPosition);

        SetRedraw();
        UpdateBuffer();
    }
    else if (m_updateInterval && m_lastUpdateTime.hasExpired(m_updateInterval))
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
    if (!m_image || m_image->isNull() || !m_browser || m_browser->hasFocus())
        return;

    QRect area = m_actualBrowserArea;
    area.translate(xoffset, yoffset);

    p->SetClipRect(clipRect);
    p->DrawImage(area.x(), area.y(), m_image, alphaMod);
}

/**
 *  \copydoc MythUIType::keyPressEvent()
 */
bool MythUIWebBrowser::keyPressEvent(QKeyEvent *event)
{
    if (!m_browser)
        return false;

    QStringList actions;
    bool handled = false;
    handled = GetMythMainWindow()->TranslateKeyPress("Browser", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "TOGGLEINPUT")
        {
            m_inputToggled = !m_inputToggled;

            if (m_inputToggled)
                slotStatusBarMessage(tr("Sending key presses to web page"));
            else
                slotStatusBarMessage(tr("Sending key presses to MythTV"));

            return true;
        }

        // if input is toggled all input goes to the web page
        if (m_inputToggled)
        {
            m_browser->keyPressEvent(event);

            return true;
        }

        QWebFrame *frame = m_browser->page()->currentFrame();
        if (action == "UP")
        {
            int pos = frame->scrollPosition().y();

            if (pos > 0)
            {
                Scroll(0, -m_actualBrowserArea.height() / 10);
            }
            else
            {
                handled = false;
            }
        }
        else if (action == "DOWN")
        {
            int pos = frame->scrollPosition().y();
            QSize maximum = frame->contentsSize() - m_actualBrowserArea.size();

            if (pos != maximum.height())
            {
                Scroll(0, m_actualBrowserArea.height() / 10);
            }
            else
            {
                handled = false;
            }
        }
        else if (action == "LEFT")
        {
            int pos = frame->scrollPosition().x();

            if (pos > 0)
            {
                Scroll(-m_actualBrowserArea.width() / 10, 0);
            }
            else
            {
                handled = false;
            }
        }
        else if (action == "RIGHT")
        {
            int pos = frame->scrollPosition().x();
            QSize maximum = frame->contentsSize() - m_actualBrowserArea.size();

            if (pos != maximum.width())
            {
                Scroll(m_actualBrowserArea.width() / 10, 0);
            }
            else
            {
                handled = false;
            }
        }
        else if (action == "PAGEUP")
        {
            Scroll(0, -m_actualBrowserArea.height());
        }
        else if (action == "PAGEDOWN")
        {
            Scroll(0, m_actualBrowserArea.height());
        }
        else if (action == "ZOOMIN")
        {
            ZoomIn();
        }
        else if (action == "ZOOMOUT")
        {
            ZoomOut();
        }
        else if (action == "MOUSEUP" || action == "MOUSEDOWN" ||
                 action == "MOUSELEFT" || action == "MOUSERIGHT" ||
                 action == "MOUSELEFTBUTTON")
        {
            HandleMouseAction(action);
        }
        else if (action == "PAGELEFT")
        {
            Scroll(-m_actualBrowserArea.width(), 0);
        }
        else if (action == "PAGERIGHT")
        {
            Scroll(m_actualBrowserArea.width(), 0);
        }
        else if ((action == "NEXTLINK")     ||
                 (action == "PREVIOUSLINK") ||
                 (action == "FOLLOWLINK"))
        {
            m_browser->keyPressEvent(event);
        }
        else if (action == "HISTORYBACK")
        {
            Back();
        }
        else if (action == "HISTORYFORWARD")
        {
            Forward();
        }
        else
        {
            handled = false;
        }
    }

    return handled;
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

            auto *me = new QMouseEvent(QEvent::MouseButtonPress, curPos,
                                              Qt::LeftButton, Qt::LeftButton,
                                              Qt::NoModifier);
            QCoreApplication::postEvent(widget, me);

            me = new QMouseEvent(QEvent::MouseButtonRelease, curPos,
                                 Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
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
        QString duration = getFirstText(element);
        m_scrollAnimation.setDuration(duration.toInt());
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
    m_scrollAnimation.setDuration(browser->m_scrollAnimation.duration());
}

/**
 *  \copydoc MythUIType::CreateCopy()
 */
void MythUIWebBrowser::CreateCopy(MythUIType *parent)
{
    auto *browser = new MythUIWebBrowser(parent, objectName());
    browser->CopyFrom(this);
}
