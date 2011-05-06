#include <stdlib.h>
#include <iostream>

// qt
#include <QEvent>
#include <QIcon>

// myth
#include "mythverbose.h"
#include "libmythui/mythmainwindow.h"

// mythbrowser
#include "mythbrowser.h"
#include "webpage.h"

using namespace std;

WebPage::WebPage(MythBrowser *parent, QRect area, const char* name)
{
    m_parent = parent;

    m_listItem = new MythUIButtonListItem(parent->m_pageList, "", "", false,
                                        MythUIButtonListItem::CantCheck, false);

    m_browser = new MythUIWebBrowser(parent, name);
    m_browser->SetArea(area);
    m_browser->Init();

    m_active = false;

    connect(m_browser, SIGNAL(loadStarted()),
            this, SLOT(slotLoadStarted()));
    connect(m_browser, SIGNAL(loadFinished(bool)),
            this, SLOT(slotLoadFinished(bool)));
    connect(m_browser, SIGNAL(loadProgress(int)),
            this, SLOT(slotLoadProgress(int)));
    connect(m_browser, SIGNAL(statusBarMessage(const QString&)),
            this, SLOT(slotStatusBarMessage(const QString&)));
    connect(m_browser, SIGNAL(titleChanged(const QString&)),
            this, SLOT(slotTitleChanged(const QString&)));
    connect(m_browser, SIGNAL(iconChanged(void)),
            this, SLOT(slotIconChanged(void)));
}

WebPage::WebPage(MythBrowser *parent, MythUIWebBrowser *browser)
{
    m_parent = parent;

    m_listItem = new MythUIButtonListItem(parent->m_pageList, "");

    m_browser = browser;

    connect(m_browser, SIGNAL(loadStarted()),
            this, SLOT(slotLoadStarted()));
    connect(m_browser, SIGNAL(loadFinished(bool)),
            this, SLOT(slotLoadFinished(bool)));
    connect(m_browser, SIGNAL(titleChanged(const QString&)),
            this, SLOT(slotTitleChanged(const QString&)));
    connect(m_browser, SIGNAL(iconChanged(void)),
            this, SLOT(slotIconChanged(void)));

    connect(m_browser, SIGNAL(loadProgress(int)),
            this, SLOT(slotLoadProgress(int)));
    connect(m_browser, SIGNAL(statusBarMessage(const QString&)),
            this, SLOT(slotStatusBarMessage(const QString&)));
}

WebPage::~WebPage()
{
    if (m_browser)
    {
        m_browser->disconnect();
        m_parent->DeleteChild(m_browser);
        m_browser = NULL;
    }

    if (m_listItem)
    {
        delete m_listItem;
        m_listItem = NULL;
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
    QIcon icon = m_browser->GetIcon();

    if (icon.isNull())
    {
        //FIXME use a default icon here?
        m_listItem->SetImage("", "favicon");
    }
    else
    {
        if (m_listItem)
        {
            QPixmap pixmap = icon.pixmap(32, 32);
            QImage image = pixmap.toImage();
            image = image.scaled(
                QSize(32,32), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            MythImage *mimage = GetMythPainter()->GetFormatImage();
            mimage->Assign(image);

            m_listItem->setImage(mimage, "favicon");
        }
    }

    m_parent->m_pageList->Refresh();
}

void WebPage::slotLoadStarted(void)
{
    m_listItem->SetText(tr("Loading..."));
    m_parent->m_pageList->Update();
}

void WebPage::slotLoadFinished(bool OK)
{
    (void) OK;

    slotLoadProgress(0);

    slotIconChanged();
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
