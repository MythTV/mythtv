#include <cstdlib>
#include <iostream>

// qt
#include <QEvent>
#include <QIcon>

// MythTV
#include <libmythbase/mythlogging.h>
#include <libmythui/mythmainwindow.h>

// mythbrowser
#include "mythbrowser.h"
#include "webpage.h"

WebPage::WebPage(MythBrowser *parent, QRect area, const char* name)
  : m_parent(parent)
{
    m_listItem = new MythUIButtonListItem(parent->m_pageList, "", "", false,
                                        MythUIButtonListItem::CantCheck, false);

    m_browser = new MythUIWebBrowser(parent, name);
    m_browser->SetArea(MythRect(area));
    m_browser->Init();

    connect(m_browser, &MythUIWebBrowser::loadStarted,
            this, &WebPage::slotLoadStarted);
    connect(m_browser, &MythUIWebBrowser::loadFinished,
            this, &WebPage::slotLoadFinished);
    connect(m_browser, &MythUIWebBrowser::loadProgress,
            this, &WebPage::slotLoadProgress);
    connect(m_browser, &MythUIWebBrowser::statusBarMessage,
            this, &WebPage::slotStatusBarMessage);
    connect(m_browser, &MythUIWebBrowser::titleChanged,
            this, &WebPage::slotTitleChanged);
}

WebPage::WebPage(MythBrowser *parent, MythUIWebBrowser *browser)
  : m_parent(parent),
    m_browser(browser)
{
    m_listItem = new MythUIButtonListItem(parent->m_pageList, "");

    connect(m_browser, &MythUIWebBrowser::loadStarted,
            this, &WebPage::slotLoadStarted);
    connect(m_browser, &MythUIWebBrowser::loadFinished,
            this, &WebPage::slotLoadFinished);
    connect(m_browser, &MythUIWebBrowser::titleChanged,
            this, &WebPage::slotTitleChanged);
    connect(m_browser, &MythUIWebBrowser::loadProgress,
            this, &WebPage::slotLoadProgress);
    connect(m_browser, &MythUIWebBrowser::statusBarMessage,
            this, &WebPage::slotStatusBarMessage);
}

WebPage::~WebPage()
{
    if (m_browser)
    {
        m_browser->disconnect();
        m_parent->DeleteChild(m_browser);
        m_browser = nullptr;
    }

    if (m_listItem)
    {
        delete m_listItem;
        m_listItem = nullptr;
    }
}

void WebPage::SetActive(bool active)
{
    if (active)
    {
        m_browser->SetActive(true);
        m_browser->Show();
    }
    else
    {
        m_browser->SetActive(false);
        m_browser->Hide();
    }

    m_active = active;
}

void WebPage::slotIconChanged(void)
{
    if (!m_listItem)
        return;

    QIcon icon = m_browser->GetIcon();

    if (icon.isNull())
    {
        MythImage *mimage = m_parent->GetDefaultFavIcon();
        m_listItem->SetImage(mimage, "favicon");
        mimage->DecrRef();
    }
    else
    {
        QPixmap pixmap = icon.pixmap(32, 32);
        QImage image = pixmap.toImage();
        image = image.scaled(
            QSize(32,32), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        MythImage *mimage = GetMythPainter()->GetFormatImage();
        mimage->Assign(image);

        m_listItem->SetImage(mimage, "favicon");
        mimage->DecrRef();
    }

    m_parent->m_pageList->Refresh();
}

void WebPage::slotLoadStarted(void)
{
    m_listItem->SetText(tr("Loading..."));
    m_listItem->DisplayState("loading", "loadingstate");
    m_listItem->SetImage(nullptr, "favicon");
    m_listItem->SetImage("", "favicon");

    m_parent->m_pageList->Update();
}

void WebPage::slotLoadFinished(bool OK)
{
    m_listItem->DisplayState("off", "loadingstate");

    slotIconChanged();

    m_listItem->SetText(m_browser->GetTitle());

    emit loadFinished(OK);
}

void WebPage::slotLoadProgress(int progress)
{
    if (m_active)
        emit loadProgress(progress);
}

void WebPage::slotStatusBarMessage(const QString &text)
{
    if (m_active)
        emit statusBarMessage(text);
}

void WebPage::slotTitleChanged(const QString &title)
{
    m_listItem->SetText(title);
    m_parent->m_pageList->Update();
}
