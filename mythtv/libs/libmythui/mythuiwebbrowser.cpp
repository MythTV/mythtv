/**
 * \file mythuiwebbrowser.cpp
 * \author Paul Harrison <mythtv@dsl.pipex.com>
 * \brief Provide a web browser widget.
 *
 * This requires qt4.4.0 or later to function properly.
 *
 */

#include "mythuiwebbrowser.h"

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

// myth
#include "mythpainter.h"
#include "mythimage.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"
#include "mythverbose.h"
#include "mythdb.h"
#include "mythdirs.h"
#include "mythuihelper.h"

/**
 * @class MythWebView
 * @brief Subclass of QWebView
 * \note allows us to intercept keypresses
 */
MythWebView::MythWebView(QWidget *parent, MythUIWebBrowser *parentBrowser)
           : QWebView(parent)
{
    m_parentBrowser = parentBrowser;

    connect(this->page(), SIGNAL(unsupportedContent(QNetworkReply *)),
            this, SLOT(handleUnsupportedContent(QNetworkReply *)));

    page()->setForwardUnsupportedContent(true);
}

/**
 *  \copydoc MythUIType::keyPressEvent()
 */
void MythWebView::keyPressEvent(QKeyEvent *event)
{
    // if the QWebView widget has focus then all keypresses from a regular keyboard
    // get sent here first
    if (m_parentBrowser && m_parentBrowser->IsInputToggled())
    {
        // input is toggled so pass all keypresses to the QWebView's handler
        QWebView::keyPressEvent(event);
    }
    else
    {
        // we need to convert a few keypress events so the QWebView does the right thing
        QStringList actions;
        bool handled = false;
        handled = GetMythMainWindow()->TranslateKeyPress("Browser", event, actions);

        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
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
            else if (action == "PREVIOUSLINK")
            {
                QKeyEvent shiftTabKey(event->type(), Qt::Key_Tab,
                                      event->modifiers() | Qt::ShiftModifier,QString(),
                                      event->isAutoRepeat(), event->count());
                *event = shiftTabKey;
                QWebView::keyPressEvent(event);
                return;
            }
            else if (action == "FOLLOWLINK")
            {
                QKeyEvent returnKey(event->type(), Qt::Key_Return,
                                    event->modifiers(), QString(),
                                    event->isAutoRepeat(), event->count());
                *event = returnKey;
                QWebView::keyPressEvent(event);
                return;
            }
        }

        // pass the keyPress event to our main window handler so they get handled properly
        // by the various mythui handlers
        QCoreApplication::postEvent(GetMythMainWindow(), new QKeyEvent(*event));
    }
}

void MythWebView::handleUnsupportedContent(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError)
    {
        return;
    }

    QString filename = "htmls/notfound.html";
    if (!GetMythUI()->FindThemeFile(filename))
        return;

    QFile file(QLatin1String(qPrintable(filename)));
    bool isOpened = file.open(QIODevice::ReadOnly);
    if (!isOpened)
        return;

    QString title = tr("Error loading page: %1").arg(reply->url().toString());
    QString html = QString(QLatin1String(file.readAll()))
                        .arg(title)
                        .arg(reply->errorString())
                        .arg(reply->url().toString());

    QBuffer imageBuffer;
    imageBuffer.open(QBuffer::ReadWrite);
    QIcon icon = style()->standardIcon(QStyle::SP_MessageBoxWarning, 0, this);
    QPixmap pixmap = icon.pixmap(QSize(32,32));
    if (pixmap.save(&imageBuffer, "PNG"))
    {
        html.replace(QLatin1String("IMAGE_BINARY_DATA_HERE"),
                     QString(QLatin1String(imageBuffer.buffer().toBase64())));
    }

    QList<QWebFrame*> frames;
    frames.append(page()->mainFrame());
    while (!frames.isEmpty())
    {
        QWebFrame *frame = frames.takeFirst();
        if (frame->url() == reply->url())
        {
            frame->setHtml(html, reply->url());
            return;
        }
        QList<QWebFrame *> children = frame->childFrames();
        foreach(QWebFrame *frame, children)
            frames.append(frame);
    }

    page()->mainFrame()->setHtml(html, reply->url());

    emit statusBarMessage(title);
}

/**
 * \class MythUIWebBrowser
 * \brief Provide a web browser widget.
 *
 * Uses QtWebKit and so requires Qt4.4.0 or later.
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
 *           <background color="white" alpha="255" />
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
    : MythUIType(parent, name), m_browser(NULL),
      m_image(NULL),         m_active(false),
      m_initialized(false),  m_lastUpdateTime(QTime()),
      m_updateInterval(0),   m_zoom(1.0),
      m_bgColor("White"),    m_widgetUrl(QUrl()), m_userCssFile(""),
      m_inputToggled(false), m_lastMouseAction(""),
      m_mouseKeyCount(0),    m_lastMouseActionTime()
{
    SetCanTakeFocus(true);
}

/**
 *  \copydoc MythUIType::Finalize()
 */
void MythUIWebBrowser::Finalize(void)
{
    Init();
    MythUIType::Finalize();
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
    if (m_initialized)
        return;

    m_browser = new MythWebView(GetMythMainWindow()->GetPaintWindow(), this);
    m_browser->setGeometry(m_Area);
    m_browser->setFixedSize(m_Area.size());
    m_browser->move(m_Area.x(), m_Area.y());
    m_browser->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);

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

    connect(m_browser, SIGNAL(loadStarted()),
            this, SLOT(slotLoadStarted()));
    connect(m_browser, SIGNAL(loadFinished(bool)),
            this, SLOT(slotLoadFinished(bool)));
    connect(m_browser, SIGNAL(loadProgress(int)),
            this, SLOT(slotLoadProgress(int)));
    connect(m_browser, SIGNAL(titleChanged(const QString&)),
            this, SLOT(slotTitleChanged(const QString&)));
    connect(m_browser, SIGNAL(iconChanged(void)),
            this, SLOT(slotIconChanged(void)));
    connect(m_browser, SIGNAL(statusBarMessage(const QString&)),
            this, SLOT(slotStatusBarMessage(const QString&)));
    connect(m_browser->page(), SIGNAL(linkHovered(const QString&, const QString&, const QString&)),
            this, SLOT(slotStatusBarMessage(const QString&)));
    connect(m_browser, SIGNAL(linkClicked(const QUrl&)),
            this, SLOT(slotLinkClicked(const QUrl&)));

    connect(this, SIGNAL(TakingFocus()),
            this, SLOT(slotTakingFocus(void)));
    connect(this, SIGNAL(LosingFocus()),
            this, SLOT(slotLosingFocus(void)));

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
    QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled,
                                                 true);

    QImage image = QImage(m_Area.size(), QImage::Format_ARGB32);
    m_image = GetMythMainWindow()->GetCurrentPainter()->GetFormatImage();
    m_image->Assign(image);

    SetBackgroundColor(m_bgColor);
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
        m_browser->disconnect();
        m_browser->deleteLater();
        m_browser = NULL;
    }

    if (m_image)
    {
        m_image->DownRef();
        m_image = NULL;
    }
}

/** \fn MythUIWebBrowser::LoadPage(QUrl)
 *  \brief Loads the specified url and displays it.
 *  \param url The url to load
 */
void MythUIWebBrowser::LoadPage(QUrl url)
{
    if (!m_browser)
        return;

    m_browser->load(url);
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

    m_browser->setHtml(html, baseUrl);
}

/** \fn MythUIWebBrowser::LoadUserStyleSheet(QUrl)
 *  \brief Sets the specified user style sheet.
 *  \param url The url to the style sheet
 */
void MythUIWebBrowser::LoadUserStyleSheet(QUrl url)
{
    if (!m_browser)
        return;

    VERBOSE(VB_IMPORTANT, "MythUIWebBrowser: Loading css from - " + url.toString());

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
    if (m_active == active)
        return;

    m_active = active;

    if (m_active)
    {
        if (m_HasFocus)
        {
            m_browser->setUpdatesEnabled(false);
            m_browser->setFocus();
            m_browser->show();
            m_browser->raise();
            m_browser->setUpdatesEnabled(true);
        }
    }
    else
    {
        if (m_HasFocus)
        {
            m_browser->clearFocus();
            m_browser->hide();
            UpdateBuffer();
        }
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

/** \fn MythUIWebBrowser::SetZoom(float)
 *  \brief Set the text size to specific size
 *  \param zoom The size to use. Useful values are between 0.2 and 5.0
 */
void MythUIWebBrowser::SetZoom(float zoom)
{
    if (!m_browser)
        return;

    m_zoom = zoom;
#if QT_VERSION >= 0x040500
    m_browser->setZoomFactor(m_zoom);
#else
    m_browser->setTextSizeMultiplier(m_zoom);
#endif
    UpdateBuffer();
}

/** \fn MythUIWebBrowser::GetZoom()
 *  \brief Get the current zoom level
 *  \return the zoom level
 */
float MythUIWebBrowser::GetZoom(void)
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
    else
        return QIcon();
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
    else
        return QUrl();
}

void MythUIWebBrowser::slotLoadStarted(void)
{
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

void MythUIWebBrowser::slotTitleChanged( const QString &title)
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

void MythUIWebBrowser::slotTakingFocus(void)
{
    if (m_active)
    {
        m_browser->setUpdatesEnabled(false);
        m_browser->setFocus();
        m_browser->show();
        m_browser->raise();
        m_browser->setUpdatesEnabled(true);
    }
    else
        UpdateBuffer();
}

void MythUIWebBrowser::slotLosingFocus(void)
{
    m_browser->clearFocus();
    m_browser->hide();

    UpdateBuffer();
}

void MythUIWebBrowser::UpdateBuffer(void)
{
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
    if (m_updateInterval && m_lastUpdateTime.elapsed() > m_updateInterval)
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
                       int alphaMod, QRect clipRegion)
{
    if (!m_image || m_image->isNull() || !m_browser || m_browser->hasFocus())
        return;

    QRect area = m_Area;
    area.translate(xoffset, yoffset);

    p->DrawImage(area.x(), area.y(), m_image, alphaMod);
}

/**
 *  \copydoc MythUIType::keyPressEvent()
 */
bool MythUIWebBrowser::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = false;
    handled = GetMythMainWindow()->TranslateKeyPress("Browser", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "TOGGLEINPUT")
        {
            m_inputToggled = !m_inputToggled;
            return true;
        }

        // if input is toggled all input goes to the web page
        if (m_inputToggled)
        {
            m_browser->keyPressEvent(event);
            return true;
        }

        if (action == "UP")
        {
            int pos = m_browser->page()->mainFrame()->scrollBarValue(
                          Qt::Vertical);
            if (pos > 0)
            {
                m_browser->page()->mainFrame()->setScrollBarValue(
                    Qt::Vertical, pos - m_Area.height() / 10);
                UpdateBuffer();
            }
            else
                handled = false;
        }
        else if (action == "DOWN")
        {
            int pos = m_browser->page()->mainFrame()->scrollBarValue(
                          Qt::Vertical);
            if (pos != m_browser->page()->mainFrame()->scrollBarMaximum(
                           Qt::Vertical))
            {
                m_browser->page()->mainFrame()->setScrollBarValue(
                    Qt::Vertical, pos + m_Area.height() / 10);
                UpdateBuffer();
            }
            else
                handled = false;
        }
        else if (action == "LEFT")
        {
            int pos = m_browser->page()->mainFrame()->scrollBarValue(
                          Qt::Horizontal);
            if (pos > 0)
            {
                m_browser->page()->mainFrame()->setScrollBarValue(
                    Qt::Horizontal, pos - m_Area.width() / 10);
                UpdateBuffer();
            }
            else
                handled = false;
        }
        else if (action == "RIGHT")
        {
            int pos = m_browser->page()->mainFrame()->scrollBarValue(
                          Qt::Horizontal);
            if (pos != m_browser->page()->mainFrame()->scrollBarMaximum(
                           Qt::Horizontal))
            {
                m_browser->page()->mainFrame()->setScrollBarValue(
                    Qt::Horizontal, pos + m_Area.width() / 10);
                UpdateBuffer();
            }
            else
                handled = false;
        }
        else if (action == "PAGEUP")
        {
            int pos = m_browser->page()->mainFrame()->scrollBarValue(
                          Qt::Vertical);
            m_browser->page()->mainFrame()->setScrollBarValue(
                Qt::Vertical, pos - m_Area.height());
            UpdateBuffer();
        }
        else if (action == "PAGEDOWN")
        {
            int pos = m_browser->page()->mainFrame()->scrollBarValue(
                          Qt::Vertical);
            m_browser->page()->mainFrame()->setScrollBarValue(
                Qt::Vertical, pos + m_Area.height());
            UpdateBuffer();
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
            int pos = m_browser->page()->mainFrame()->scrollBarValue(
                Qt::Horizontal);
            m_browser->page()->mainFrame()->setScrollBarValue(
                Qt::Horizontal, pos - m_Area.width());
            UpdateBuffer();
        }
        else if (action == "PAGERIGHT")
        {
            int pos = m_browser->page()->mainFrame()->scrollBarValue(
                Qt::Horizontal);
            m_browser->page()->mainFrame()->setScrollBarValue(
                Qt::Horizontal, pos + m_Area.width());
            UpdateBuffer();
        }
        else if (action == "NEXTLINK")
        {
            m_browser->keyPressEvent(event);
        }
        else if (action == "PREVIOUSLINK")
        {
            m_browser->keyPressEvent(event);
        }
        else if (action == "FOLLOWLINK")
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
            handled = false;
    }

    return handled;
}

void MythUIWebBrowser::HandleMouseAction(const QString &action)
{
    int step = 5;

    // speed up mouse movement if the same key is held down
    if (action == m_lastMouseAction &&
           m_lastMouseActionTime.msecsTo(QTime::currentTime()) < 500)
    {
        m_lastMouseActionTime = QTime::currentTime();
        m_mouseKeyCount++;
        if (m_mouseKeyCount > 5)
            step = 25;
    }
    else
    {
        m_lastMouseAction = action;
        m_lastMouseActionTime = QTime::currentTime();
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

            QMouseEvent *me = new QMouseEvent(QEvent::MouseButtonPress, curPos,
                                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::postEvent(widget, me);

            me = new QMouseEvent(QEvent::MouseButtonRelease, curPos,
                                Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::postEvent(widget, me);
        }
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
        m_zoom = zoom.toFloat();
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
    MythUIWebBrowser *browser = dynamic_cast<MythUIWebBrowser *>(base);
    if (!browser)
    {
        VERBOSE(VB_IMPORTANT, "ERROR, bad parsing");
        return;
    }

    MythUIType::CopyFrom(base);

    m_zoom = browser->m_zoom;
    m_bgColor = browser->m_bgColor;
    m_widgetUrl = browser->m_widgetUrl;
    m_userCssFile = browser->m_userCssFile;
    m_updateInterval = browser->m_updateInterval;

    Init();
}

/**
 *  \copydoc MythUIType::CreateCopy()
 */
void MythUIWebBrowser::CreateCopy(MythUIType *parent)
{
    MythUIWebBrowser *browser = new MythUIWebBrowser(parent, objectName());
    browser->CopyFrom(this);
}
