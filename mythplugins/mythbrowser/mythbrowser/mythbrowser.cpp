#include <stdlib.h>
#include <iostream>

// qt
#include <QEvent>

// myth
#include "mythlogging.h"
#include "mythcontext.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"

// mythbrowser
#include "webpage.h"
#include "bookmarkeditor.h"
#include "mythbrowser.h"


using namespace std;

MythBrowser::MythBrowser(MythScreenStack *parent,
                         QStringList &urlList, float zoom)
    : MythScreenType (parent, "mythbrowser"),
      m_urlList(urlList),  m_pageList(NULL),
      m_progressBar(NULL), m_titleText(NULL),
      m_statusText(NULL),  m_currentBrowser(-1),
      m_zoom(zoom),        m_menuPopup(NULL),
      m_defaultFavIcon(NULL)
{
    GetMythMainWindow()->PauseIdleTimer(true);
}

MythBrowser::~MythBrowser()
{
    while (!m_browserList.isEmpty())
        delete m_browserList.takeFirst();
    GetMythMainWindow()->PauseIdleTimer(false);
    if (m_defaultFavIcon)
    {
        m_defaultFavIcon->DecrRef();
        m_defaultFavIcon = NULL;
    }
}

bool MythBrowser::Create(void)
{
    // Load the theme for this screen
    if (!LoadWindowFromXML("browser-ui.xml", "browser", this))
        return false;

    bool err = false;
    MythUIWebBrowser *browser = NULL;

    UIUtilE::Assign(this, browser,         "webbrowser", &err);
    UIUtilE::Assign(this, m_pageList,      "pagelist", &err);
    UIUtilW::Assign(this, m_progressBar,   "progressbar");
    UIUtilW::Assign(this, m_statusText,    "status");
    UIUtilW::Assign(this, m_titleText,     "title");
    UIUtilW::Assign(this, m_backButton,    "back");
    UIUtilW::Assign(this, m_forwardButton, "forward");
    UIUtilW::Assign(this, m_exitButton,    "exit");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'browser'");
        return false;
    }

    connect(m_pageList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(slotTabSelected(MythUIButtonListItem*)));

    // create the default favicon
    QString favIcon = "mb_default_favicon.png";
    GetMythUI()->FindThemeFile(favIcon);
    if (QFile::exists(favIcon))
    {
        QImage image(favIcon);
        m_defaultFavIcon = GetMythPainter()->GetFormatImage();
        m_defaultFavIcon->Assign(image);
    }

    // this is the template for all other browser tabs
    WebPage *page = new WebPage(this, browser);

    m_browserList.append(page);
    page->getBrowser()->SetZoom(m_zoom);
    page->getBrowser()->SetDefaultSaveDirectory(m_defaultSaveDir);
    page->getBrowser()->SetDefaultSaveFilename(m_defaultSaveFilename);

    page->SetActive(true);

    connect(page, SIGNAL(loadProgress(int)),
            this, SLOT(slotLoadProgress(int)));
    connect(page, SIGNAL(statusBarMessage(const QString&)),
            this, SLOT(slotStatusBarMessage(const QString&)));
    connect(page, SIGNAL(loadFinished(bool)),
            this, SLOT(slotLoadFinished(bool)));

    if (m_progressBar)
        m_progressBar->SetTotal(100);

    if (m_exitButton)
    {
        m_exitButton->SetEnabled(false);
        m_exitButton->SetEnabled(true);
        connect(m_exitButton, SIGNAL(Clicked()), this, SLOT(Close()));
    }

    if (m_backButton)
    {
        m_backButton->SetEnabled(false);
        connect(m_backButton, SIGNAL(Clicked()), this, SLOT(slotBack()));
    }

    if (m_forwardButton)
    {
        m_forwardButton->SetEnabled(false);
        connect(m_forwardButton, SIGNAL(Clicked()), this, SLOT(slotForward()));
    }

    BuildFocusList();

    SetFocusWidget(browser);

    slotOpenURL(m_urlList[0]);

    for (int x = 1; x < m_urlList.size(); x++)
        slotAddTab(m_urlList[x], false);

    switchTab(0);

    return true;
}

MythUIWebBrowser* MythBrowser::activeBrowser(void)
{
    if (m_currentBrowser >=0 && m_currentBrowser < m_browserList.size())
        return m_browserList[m_currentBrowser]->getBrowser();
    else
        return m_browserList[0]->getBrowser();
}

void MythBrowser::slotEnterURL(void)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString message = tr("Enter URL");


    MythTextInputDialog *dialog = new MythTextInputDialog(popupStack, message);

    if (dialog->Create())
       popupStack->AddScreen(dialog);

     connect(dialog, SIGNAL(haveResult(QString)),
            SLOT(slotOpenURL(QString)), Qt::QueuedConnection);
}

void MythBrowser::slotAddTab(const QString &url, bool doSwitch)
{
    QString name = QString("browser%1").arg(m_browserList.size() + 1);
    WebPage *page = new WebPage(this, m_browserList[0]->getBrowser()->GetArea(),
                                name.toAscii().constData());
    page->getBrowser()->SetZoom(m_zoom);

    m_browserList.append(page);

    QString newUrl = url;

    if (newUrl.isEmpty())
        newUrl = "http://www.google.com"; // TODO: add a user definable home page

    if (!newUrl.startsWith("http://") && !newUrl.startsWith("https://") &&
            !newUrl.startsWith("file:/") )
        newUrl.prepend("http://");
    page->getBrowser()->LoadPage(QUrl::fromEncoded(newUrl.toLocal8Bit()));

    page->SetActive(false);

    connect(page, SIGNAL(loadProgress(int)),
            this, SLOT(slotLoadProgress(int)));
    connect(page, SIGNAL(statusBarMessage(const QString&)),
            this, SLOT(slotStatusBarMessage(const QString&)));
    connect(page, SIGNAL(loadFinished(bool)),
            this, SLOT(slotLoadFinished(bool)));

    if (doSwitch)
        m_pageList->SetItemCurrent(m_browserList.size() -1);
}

void MythBrowser::slotDeleteTab(void)
{
    if (m_browserList.size() < 2)
        return;

    if (m_currentBrowser >= 0 && m_currentBrowser < m_browserList.size())
    {
        int tab = m_currentBrowser;
        m_currentBrowser = -1;
        WebPage *page = m_browserList.takeAt(tab);
        delete page;

        if (tab >= m_browserList.size())
            tab = m_browserList.size() - 1;

        switchTab(tab);
    }
}

void MythBrowser::switchTab(int newTab)
{
    if (newTab == m_currentBrowser)
        return;

    if (newTab < 0 || newTab >= m_browserList.size())
        return;

    if (m_currentBrowser >= 0 && m_currentBrowser < m_browserList.size())
        m_browserList[m_currentBrowser]->SetActive(false);

    BuildFocusList();

    m_browserList[newTab]->SetActive(true);

    m_currentBrowser = newTab;

    if (GetFocusWidget() != m_pageList)
        SetFocusWidget(activeBrowser());
}

void MythBrowser::slotOpenURL(const QString &url)
{
    QString sUrl = url;
    if (!sUrl.startsWith("http://") && !sUrl.startsWith("https://") &&
            !sUrl.startsWith("file:/") )
        sUrl.prepend("http://");

    activeBrowser()->LoadPage(QUrl::fromEncoded(sUrl.toLocal8Bit()));
}

void MythBrowser::slotZoomOut()
{
    activeBrowser()->ZoomOut();
}

void MythBrowser::slotZoomIn()
{
    activeBrowser()->ZoomIn();
}

void MythBrowser::slotBack()
{
    activeBrowser()->Back();
}

void MythBrowser::slotForward()
{
    activeBrowser()->Forward();
}

void MythBrowser::slotAddBookmark()
{
    m_editBookmark.category = "";
    m_editBookmark.name = m_pageList->GetValue();
    m_editBookmark.url = activeBrowser()->GetUrl().toString();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    BookmarkEditor *editor = new BookmarkEditor(&m_editBookmark,
            true, mainStack, "bookmarkeditor");


    if (editor->Create())
        mainStack->AddScreen(editor);
}

void MythBrowser::slotLoadStarted(void)
{
    MythUIButtonListItem *item = m_pageList->GetItemCurrent();
    if (item)
        item->SetText(tr("Loading..."));
}

void MythBrowser::slotLoadFinished(bool OK)
{
    (void) OK;

    if (m_progressBar)
        m_progressBar->SetUsed(0);

    if (m_backButton)
        m_backButton->SetEnabled(activeBrowser()->CanGoBack());

    if (m_forwardButton)
        m_forwardButton->SetEnabled(activeBrowser()->CanGoForward());
}

void MythBrowser::slotLoadProgress(int progress)
{
    if (m_progressBar)
        m_progressBar->SetUsed(progress);
}

void MythBrowser::slotTitleChanged(const QString &title)
{
    MythUIButtonListItem *item = m_pageList->GetItemCurrent();
    if (item)
        item->SetText(title);
}

void MythBrowser::slotStatusBarMessage(const QString &text)
{
    if (m_statusText)
        m_statusText->SetText(text);
}

void MythBrowser::slotTabSelected(MythUIButtonListItem *item)
{
    if (!item)
        return;

    switchTab(m_pageList->GetCurrentPos());
    slotStatusBarMessage(item->GetText());
}

void MythBrowser::slotTabLosingFocus(void)
{
    SetFocusWidget(activeBrowser());
}

bool MythBrowser::keyPressEvent(QKeyEvent *event)
{
    // Always send keypress events to the currently focused widget first
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Browser", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {

        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            slotStatusBarMessage("");

            QString label = tr("Actions");

            MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

            m_menuPopup = new MythDialogBox(label, popupStack, "actionmenu");

            if (m_menuPopup->Create())
                popupStack->AddScreen(m_menuPopup);

            m_menuPopup->SetReturnEvent(this, "action");

            m_menuPopup->AddButton(tr("Enter URL"), SLOT(slotEnterURL()));

            if (activeBrowser()->CanGoBack())
                m_menuPopup->AddButton(tr("Back"), SLOT(slotBack()));

            if (activeBrowser()->CanGoForward())
                m_menuPopup->AddButton(tr("Forward"), SLOT(slotForward()));

            m_menuPopup->AddButton(tr("Zoom In"), SLOT(slotZoomIn()));
            m_menuPopup->AddButton(tr("Zoom Out"), SLOT(slotZoomOut()));
            m_menuPopup->AddButton(tr("New Tab"), SLOT(slotAddTab()));

            if (m_browserList.size() > 1)
                m_menuPopup->AddButton(tr("Delete Tab"), SLOT(slotDeleteTab()));

            m_menuPopup->AddButton(tr("Add Bookmark"), SLOT(slotAddBookmark()));
        }
        else if (action == "INFO")
        {
            if (GetFocusWidget() == m_pageList)
                SetFocusWidget(activeBrowser());
            else
                SetFocusWidget(m_pageList);
        }
        else if (action == "ESCAPE")
        {
            GetScreenStack()->PopScreen(true, true);
        }
        else if (action == "PREVTAB")
        {
            int pos = m_pageList->GetCurrentPos();
            if (pos > 0)
               m_pageList->SetItemCurrent(--pos);
        }
        else if (action == "NEXTTAB")
        {
            int pos = m_pageList->GetCurrentPos();
            if (pos < m_pageList->GetCount() - 1)
               m_pageList->SetItemCurrent(++pos);
        }
        else if (action == "DELETE")
        {
            slotDeleteTab();
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}


