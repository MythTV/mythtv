// POSIX headers
#include <unistd.h>

// C headers
#include <cmath>

// QT headers
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QTimer>
#include <QRegExp>
#include <QUrl>

// MythTV headers
#include <mythuibuttonlist.h>
#include <mythmainwindow.h>
#include <mythdialogbox.h>
#include <mythcontext.h>
#include <mythuiimage.h>
#include <mythsystemlegacy.h>
#include <mythuitext.h>
#include <mythdate.h>
#include <mythdirs.h>
#include <mythdb.h>
#include <mythdownloadmanager.h>

// MythNews headers
#include "mythnews.h"
#include "mythnewseditor.h"
#include "newsdbutil.h"
#include "mythnewsconfig.h"

#define LOC      QString("MythNews: ")
#define LOC_WARN QString("MythNews, Warning: ")
#define LOC_ERR  QString("MythNews, Error: ")

/** \brief Creates a new MythNews Screen
 *  \param parent Pointer to the screen stack
 *  \param name The name of the window
 */
MythNews::MythNews(MythScreenStack *parent, const QString &name) :
    MythScreenType(parent, name),
    m_RetrieveTimer(new QTimer(this)),
    m_UpdateFreq(gCoreContext->GetNumSetting("NewsUpdateFrequency", 30)),
    m_zoom(gCoreContext->GetSetting("WebBrowserZoomLevel", "1.0")),
    m_browser(gCoreContext->GetSetting("WebBrowserCommand", ""))
{
    // Setup cache directory

    QString fileprefix = GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);
    fileprefix += "/MythNews";
    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    connect(m_RetrieveTimer, SIGNAL(timeout()),
            this, SLOT(slotRetrieveNews()));

    m_RetrieveTimer->stop();
    m_RetrieveTimer->setSingleShot(false);
    m_RetrieveTimer->start(m_TimerTimeout);
}

MythNews::~MythNews()
{
    QMutexLocker locker(&m_lock);
}

bool MythNews::Create(void)
{
    QMutexLocker locker(&m_lock);

    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("news-ui.xml", "news", this);
    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_sitesList, "siteslist", &err);
    UIUtilE::Assign(this, m_articlesList, "articleslist", &err);
    UIUtilE::Assign(this, m_titleText, "title", &err);
    UIUtilE::Assign(this, m_descText, "description", &err);

    // these are all optional
    UIUtilW::Assign(this, m_nositesText, "nosites", &err);
    UIUtilW::Assign(this, m_updatedText, "updated", &err);
    UIUtilW::Assign(this, m_thumbnailImage, "thumbnail", &err);
    UIUtilW::Assign(this, m_enclosureImage, "enclosures", &err);
    UIUtilW::Assign(this, m_downloadImage, "download", &err);
    UIUtilW::Assign(this, m_podcastImage, "ispodcast", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'news'");
        return false;
    }

    if (m_nositesText)
    {
        m_nositesText->SetText(tr("You haven't configured MythNews to use any sites."));
        m_nositesText->Hide();
    }

    BuildFocusList();

    SetFocusWidget(m_sitesList);

    loadSites();
    updateInfoView(m_sitesList->GetItemFirst());

    connect(m_sitesList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT( slotSiteSelected(MythUIButtonListItem*)));
    connect(m_articlesList, SIGNAL(itemSelected( MythUIButtonListItem*)),
            this, SLOT( updateInfoView(MythUIButtonListItem*)));
    connect(m_articlesList, SIGNAL(itemClicked( MythUIButtonListItem*)),
            this, SLOT( slotViewArticle(MythUIButtonListItem*)));

    return true;
}

void MythNews::clearSites(void)
{
    m_NewsSites.clear();
    m_sitesList->Reset();
    m_articles.clear();
    m_articlesList->Reset();

    m_titleText->Reset();
    m_descText->Reset();

    if (m_updatedText)
        m_updatedText->Reset();

    if (m_downloadImage)
        m_downloadImage->Hide();

    if (m_enclosureImage)
        m_enclosureImage->Hide();

    if (m_podcastImage)
        m_podcastImage->Hide();

    if (m_thumbnailImage)
        m_thumbnailImage->Hide();
}

void MythNews::loadSites(void)
{
    QMutexLocker locker(&m_lock);

    clearSites();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT name, url, ico, updated, podcast "
        "FROM newssites "
        "ORDER BY name");

    if (!query.exec())
    {
        MythDB::DBError(LOC_ERR + "Could not load sites from DB", query);
        return;
    }

    while (query.next())
    {
        QString name = query.value(0).toString();
        QString url  = query.value(1).toString();
        QString icon = query.value(2).toString();
#if QT_VERSION < QT_VERSION_CHECK(5,8,0)
        QDateTime time = MythDate::fromTime_t(query.value(3).toUInt());
#else
        QDateTime time = MythDate::fromSecsSinceEpoch(query.value(3).toLongLong());
#endif
        bool podcast = query.value(4).toBool();
        m_NewsSites.push_back(new NewsSite(name, url, time, podcast));
    }
    std::sort(m_NewsSites.begin(), m_NewsSites.end(), NewsSite::sortByName);

    NewsSite::List::iterator it = m_NewsSites.begin();
    for (; it != m_NewsSites.end(); ++it)
    {
        MythUIButtonListItem *item =
            new MythUIButtonListItem(m_sitesList, (*it)->name());
        item->SetData(qVariantFromValue(*it));

        connect(*it, SIGNAL(finished(NewsSite*)),
                this, SLOT(slotNewsRetrieved(NewsSite*)));
    }

    slotRetrieveNews();

    if (m_nositesText)
    {
        if (m_NewsSites.empty())
            m_nositesText->Show();
        else
            m_nositesText->Hide();
    }
}

void MythNews::updateInfoView(MythUIButtonListItem *selected)
{
    QMutexLocker locker(&m_lock);

    if (!selected)
        return;

    NewsSite *site = nullptr;
    NewsArticle article;

    if (GetFocusWidget() == m_articlesList)
    {
        article = m_articles[selected];
        if (m_sitesList->GetItemCurrent())
            site = m_sitesList->GetItemCurrent()->GetData().value<NewsSite*>();
    }
    else
    {
        site = selected->GetData().value<NewsSite*>();
        if (m_articlesList->GetItemCurrent())
            article = m_articles[m_articlesList->GetItemCurrent()];
    }

    if (GetFocusWidget() == m_articlesList)
    {
        if (!article.title().isEmpty())
        {

            if (m_titleText)
                m_titleText->SetText(article.title());

            if (m_descText)
            {
                QString artText = article.description();
                // replace a few HTML characters
                artText.replace("&#8232;", "");   // LSEP
                artText.replace("&#8233;", "");   // PSEP
                artText.replace("&#163;",  u8"\u00A3");  // POUND
                artText.replace("&#173;",  "");   // ?
                artText.replace("&#8211;", "-");  // EN-DASH
                artText.replace("&#8220;", """"); // LEFT-DOUBLE-QUOTE
                artText.replace("&#8221;", """"); // RIGHT-DOUBLE-QUOTE
                artText.replace("&#8216;", "'");  // LEFT-SINGLE-QUOTE
                artText.replace("&#8217;", "'");  // RIGHT-SINGLE-QUOTE
                // Replace paragraph and break HTML with newlines
                if( artText.contains(QRegExp("</(p|P)>")) )
                {
                    artText.replace( QRegExp("<(p|P)>"), "");
                    artText.replace( QRegExp("</(p|P)>"), "\n\n");
                }
                else
                {
                    artText.replace( QRegExp("<(p|P)>"), "\n\n");
                    artText.replace( QRegExp("</(p|P)>"), "");
                }
                artText.replace( QRegExp("<(br|BR|)/>"), "\n");
                artText.replace( QRegExp("<(br|BR|)>"), "\n");
                // These are done instead of simplifyWhitespace
                // because that function also strips out newlines
                // Replace tab characters with nothing
                artText.replace( QRegExp("\t"), "");
                // Replace double space with single
                artText.replace( QRegExp("  "), "");
                // Replace whitespace at beginning of lines with newline
                artText.replace( QRegExp("\n "), "\n");
                // Remove any remaining HTML tags
                QRegExp removeHTML(QRegExp("</?.+>"));
                removeHTML.setMinimal(true);
                artText.remove((const QRegExp&) removeHTML);
                artText = artText.trimmed();
                m_descText->SetText(artText);
            }

            if (!article.thumbnail().isEmpty())
            {
                if (m_thumbnailImage)
                {
                    m_thumbnailImage->SetFilename(article.thumbnail());
                    m_thumbnailImage->Load();

                    if (!m_thumbnailImage->IsVisible())
                        m_thumbnailImage->Show();
                }
            }
            else
            {
                if (site && !site->imageURL().isEmpty())
                {
                    if (m_thumbnailImage)
                    {
                        m_thumbnailImage->SetFilename(site->imageURL());
                        m_thumbnailImage->Load();

                        if (!m_thumbnailImage->IsVisible())
                            m_thumbnailImage->Show();
                    }
                }
                else
                {
                    if (m_thumbnailImage)
                        m_thumbnailImage->Hide();
                }
            }

            if (m_downloadImage)
            {
                if (!article.enclosure().isEmpty())
                {
                    if (!m_downloadImage->IsVisible())
                        m_downloadImage->Show();
                }
                else
                    m_downloadImage->Hide();
            }

            if (m_enclosureImage)
            {
                if (!article.enclosure().isEmpty())
                {
                    if (!m_enclosureImage->IsVisible())
                        m_enclosureImage->Show();
                }
                else
                    m_enclosureImage->Hide();
            }

            if (m_podcastImage)
                m_podcastImage->Hide();
        }
    }
    else
    {
        if (m_downloadImage)
            m_downloadImage->Hide();

        if (m_enclosureImage)
            m_enclosureImage->Hide();

        if (m_podcastImage)
            m_podcastImage->Hide();

        if (site)
        {
            if (m_titleText)
                m_titleText->SetText(site->name());

            if (m_descText)
                m_descText->SetText(site->description());

            if (m_thumbnailImage && m_thumbnailImage->IsVisible())
                m_thumbnailImage->Hide();

            if (m_podcastImage && site->podcast())
                m_podcastImage->Show();

            if (!site->imageURL().isEmpty())
            {
                if (m_thumbnailImage)
                {
                    m_thumbnailImage->SetFilename(site->imageURL());
                    m_thumbnailImage->Load();

                    if (!m_thumbnailImage->IsVisible())
                        m_thumbnailImage->Show();
                }
            }
        }
    }

    if (m_updatedText)
    {

        if (site)
        {
            QString text(tr("Updated") + " - ");
            QDateTime updated(site->lastUpdated());
            if (updated.isValid()) {
                text += MythDate::toString(site->lastUpdated(),
                                           MythDate::kDateTimeFull | MythDate::kSimplify);
            }
            else
                text += tr("Unknown");
            m_updatedText->SetText(text);
        }
    }
}

QString MythNews::formatSize(long long bytes, int prec)
{
    long long sizeKB = bytes / 1024;

    if (sizeKB>1024*1024*1024) // Terabytes
    {
        double sizeGB = sizeKB/(1024*1024*1024.0);
        return QString("%1 TB").arg(sizeGB, 0, 'f', (sizeGB>10)?0:prec);
    }
    if (sizeKB>1024*1024) // Gigabytes
    {
        double sizeGB = sizeKB/(1024*1024.0);
        return QString("%1 GB").arg(sizeGB, 0, 'f', (sizeGB>10)?0:prec);
    }
    if (sizeKB>1024) // Megabytes
    {
        double sizeMB = sizeKB/1024.0;
        return QString("%1 MB").arg(sizeMB, 0, 'f', (sizeMB>10)?0:prec);
    }
    // Kilobytes
    return QString("%1 KB").arg(sizeKB);
}

bool MythNews::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("News", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "RETRIEVENEWS")
            slotRetrieveNews();
        else if (action == "CANCEL")
            cancelRetrieve();
        else if (action == "MENU")
            ShowMenu();
        else if (action == "EDIT")
            ShowEditDialog(true);
        else if (action == "DELETE")
            deleteNewsSite();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void MythNews::slotRetrieveNews(void)
{
    QMutexLocker locker(&m_lock);

    if (m_NewsSites.empty())
        return;

    m_RetrieveTimer->stop();

    NewsSite::List::iterator it = m_NewsSites.begin();
    for (; it != m_NewsSites.end(); ++it)
    {
        if ((*it)->timeSinceLastUpdate() > m_UpdateFreq)
            (*it)->retrieve();
        else
            processAndShowNews(*it);
    }

    m_RetrieveTimer->stop();
    m_RetrieveTimer->setSingleShot(false);
    m_RetrieveTimer->start(m_TimerTimeout);
}

void MythNews::slotNewsRetrieved(NewsSite *site)
{
#if QT_VERSION < QT_VERSION_CHECK(5,8,0)
    unsigned int updated = site->lastUpdated().toTime_t();
#else
    qint64 updated = site->lastUpdated().toSecsSinceEpoch();
#endif

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE newssites SET updated = :UPDATED "
                  "WHERE name = :NAME ;");
    query.bindValue(":UPDATED", updated);
    query.bindValue(":NAME", site->name());
    if (!query.exec() || !query.isActive())
        MythDB::DBError("news update time", query);

    processAndShowNews(site);
}

void MythNews::cancelRetrieve(void)
{
    QMutexLocker locker(&m_lock);

    NewsSite::List::iterator it = m_NewsSites.begin();
    for (; it != m_NewsSites.end(); ++it)
    {
        (*it)->stop();
        processAndShowNews(*it);
    }
}

void MythNews::processAndShowNews(NewsSite *site)
{
    QMutexLocker locker(&m_lock);

    if (!site)
        return;

    site->process();

    MythUIButtonListItem *siteUIItem = m_sitesList->GetItemCurrent();
    if (!siteUIItem)
        return;

    if (site != siteUIItem->GetData().value<NewsSite*>())
        return;

    QString currItem = m_articlesList->GetValue();
    int topPos = m_articlesList->GetTopItemPos();

    m_articlesList->Reset();
    m_articles.clear();

    NewsArticle::List articles = site->GetArticleList();
    NewsArticle::List::iterator it = articles.begin();
    for (; it != articles.end(); ++it)
    {
        MythUIButtonListItem *item =
            new MythUIButtonListItem(m_articlesList, (*it).title());
        m_articles[item] = *it;
    }

    if (m_articlesList->MoveToNamedPosition(currItem))
        m_articlesList->SetItemCurrent(m_articlesList->GetCurrentPos(), topPos);
}

void MythNews::slotSiteSelected(MythUIButtonListItem *item)
{
    QMutexLocker locker(&m_lock);

    if (!item || item->GetData().isNull())
        return;

    NewsSite *site = item->GetData().value<NewsSite*>();
    if (!site)
        return;

    m_articlesList->Reset();
    m_articles.clear();

    NewsArticle::List articles = site->GetArticleList();
    NewsArticle::List::iterator it = articles.begin();
    for (; it != articles.end(); ++it)
    {
        MythUIButtonListItem *blitem =
            new MythUIButtonListItem(m_articlesList, (*it).title());
        m_articles[blitem] = *it;
    }

    updateInfoView(item);
}

void MythNews::slotViewArticle(MythUIButtonListItem *articlesListItem)
{
    QMutexLocker locker(&m_lock);

    QMap<MythUIButtonListItem*,NewsArticle>::const_iterator it =
        m_articles.find(articlesListItem);

    if (it == m_articles.end())
        return;

    const NewsArticle article = *it;

    if (article.articleURL().isEmpty())
        return;

    if (article.enclosure().isEmpty())
    {
        QString cmdUrl(article.articleURL());

        if (m_browser.isEmpty())
        {
            ShowOkPopup(tr("No browser command set! MythNews needs MythBrowser to be installed."));
            return;
        }

        // display the web page
        if (m_browser.toLower() == "internal")
        {
            GetMythMainWindow()->HandleMedia("WebBrowser", cmdUrl);
            return;
        }

        QString cmd = m_browser;
        cmd.replace("%ZOOM%", m_zoom);
        cmd.replace("%URL%", cmdUrl);
        cmd.replace('\'', "%27");
        cmd.replace("&","\\&");
        cmd.replace(";","\\;");

        GetMythMainWindow()->AllowInput(false);
        myth_system(cmd, kMSDontDisableDrawing);
        GetMythMainWindow()->AllowInput(true);
        return;
    }

    playVideo(article);
}

void MythNews::ShowEditDialog(bool edit)
{
    QMutexLocker locker(&m_lock);

    NewsSite *site = nullptr;

    if (edit)
    {
        MythUIButtonListItem *siteListItem = m_sitesList->GetItemCurrent();

        if (!siteListItem || siteListItem->GetData().isNull())
            return;

        site = siteListItem->GetData().value<NewsSite*>();
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythNewsEditor *mythnewseditor = new MythNewsEditor(site, edit, mainStack,
                                                        "mythnewseditor");

    if (mythnewseditor->Create())
    {
        connect(mythnewseditor, SIGNAL(Exiting()), SLOT(loadSites()));
        mainStack->AddScreen(mythnewseditor);
    }
    else
        delete mythnewseditor;
}

void MythNews::ShowFeedManager()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythNewsConfig *mythnewsconfig = new MythNewsConfig(mainStack,
                                                        "mythnewsconfig");

    if (mythnewsconfig->Create())
    {
        connect(mythnewsconfig, SIGNAL(Exiting()), SLOT(loadSites()));
        mainStack->AddScreen(mythnewsconfig);
    }
    else
        delete mythnewsconfig;
}

void MythNews::ShowMenu(void)
{
    QMutexLocker locker(&m_lock);

    QString label = tr("Options");

    MythScreenStack *popupStack =
                            GetMythMainWindow()->GetStack("popup stack");

    m_menuPopup = new MythDialogBox(label, popupStack, "mythnewsmenupopup");

    if (m_menuPopup->Create())
    {
        popupStack->AddScreen(m_menuPopup);

        m_menuPopup->SetReturnEvent(this, "options");

        m_menuPopup->AddButton(tr("Manage Feeds"));
        m_menuPopup->AddButton(tr("Add Feed"));
        if (!m_NewsSites.empty())
        {
            m_menuPopup->AddButton(tr("Edit Feed"));
            m_menuPopup->AddButton(tr("Delete Feed"));
        }
    }
    else
    {
        delete m_menuPopup;
        m_menuPopup = nullptr;
    }
}

void MythNews::deleteNewsSite(void)
{
    QMutexLocker locker(&m_lock);

    MythUIButtonListItem *siteUIItem = m_sitesList->GetItemCurrent();

    if (siteUIItem && !siteUIItem->GetData().isNull())
    {
        NewsSite *site = siteUIItem->GetData().value<NewsSite*>();
        if (site)
        {
            removeFromDB(site->name());
            loadSites();
        }
    }
}

// does not need locking
void MythNews::playVideo(const NewsArticle &article)
{
    GetMythMainWindow()->HandleMedia("Internal", article.enclosure(),
                                     article.description(), article.title());
}

// does not need locking
void MythNews::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto dce = dynamic_cast<DialogCompletionEvent*>(event);
        if (dce == nullptr)
            return;

        QString resultid  = dce->GetId();
        int     buttonnum = dce->GetResult();

        if (resultid == "options")
        {
            if (buttonnum == 0)
                ShowFeedManager();
            else if (buttonnum == 1)
                ShowEditDialog(false);
            else if (buttonnum == 2)
                ShowEditDialog(true);
            else if (buttonnum == 3)
                deleteNewsSite();
        }

        m_menuPopup = nullptr;
    }
}
