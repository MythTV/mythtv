// qt
#include <QDateTime>
#include <QFileInfo>
#include <QImageReader>
#include <QMetaType>
#include <QString>

// MythTV
#include <libmythbase/mythdb.h>
#include <libmythbase/mythdbcon.h>
#include <libmythbase/mythdirs.h>
#include <libmythbase/mythlogging.h>
#include <libmythbase/mythsorthelper.h>
#include <libmythbase/mythsystemlegacy.h>
#include <libmythbase/netgrabbermanager.h>
#include <libmythbase/netutils.h>
#include <libmythbase/remotefile.h>
#include <libmythbase/rssparse.h>
#include <libmythbase/storagegroup.h>
#include <libmythmetadata/metadataimagedownload.h>
#include <libmythmetadata/videoutils.h>
#include <libmythui/mythprogressdialog.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuitext.h>

#include "netcommon.h"
#include "netsearch.h"
#include "rsseditor.h"
#include "searcheditor.h"

// ---------------------------------------------------

NetSearch::NetSearch(MythScreenStack *parent, const char *name)
    : NetBase(parent, name),
      m_mythXML(GetMythXMLURL())
{
}

bool NetSearch::Create()
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("netvision-ui.xml", "netsearch", this);

    if (!foundtheme)
        return false;

    m_siteList = dynamic_cast<MythUIButtonList *> (GetChild("sites"));
    m_searchResultList = dynamic_cast<MythUIButtonList *> (GetChild("results"));

    m_pageText = dynamic_cast<MythUIText *> (GetChild("page"));
    m_noSites = dynamic_cast<MythUIText *> (GetChild("nosites"));

    m_thumbImage = dynamic_cast<MythUIImage *> (GetChild("preview"));

    m_downloadable = dynamic_cast<MythUIStateType *> (GetChild("downloadable"));

    m_progress = dynamic_cast<MythUIProgressBar *> (GetChild("progress"));

    if (m_progress)
        m_progress->SetVisible(false);

    if (m_noSites)
        m_noSites->SetVisible(false);

    m_search = dynamic_cast<MythUITextEdit *> (GetChild("search"));

    if (!m_siteList || !m_searchResultList || !m_search)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing critical theme elements.");
        return false;
    }

    m_search->SetMaxLength(255);

    // UI Hookups
    connect(m_siteList, &MythUIButtonList::itemSelected,
            this, &NetSearch::SlotItemChanged);
    connect(m_siteList, &MythUIButtonList::itemClicked,
            this, &NetSearch::DoSearch);
    connect(m_searchResultList, &MythUIButtonList::itemClicked,
            this, &NetSearch::StreamWebVideo);
    connect(m_searchResultList, &MythUIButtonList::itemSelected,
            this, &NetSearch::SlotItemChanged);

    BuildFocusList();

    LoadInBackground();

    return true;
}

void NetSearch::Load()
{
    m_grabberList = findAllDBSearchGrabbers(VIDEO_FILE);
}

NetSearch::~NetSearch()
{
    if (m_netSearch)
    {
        m_netSearch->disconnect();
        m_netSearch->deleteLater();
        m_netSearch = nullptr;
    }
}

void NetSearch::LoadData(void)
{
    FillGrabberButtonList();

    if (m_grabberList.count() == 0 && m_noSites)
        m_noSites->SetVisible(true);
    else if (m_noSites)
        m_noSites->SetVisible(false);

    if (m_grabberList.isEmpty())
        RunSearchEditor();
}

bool NetSearch::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Internet Video",
                                                          event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "MENU")
            ShowMenu();
        else if (action == "PAGELEFT" && m_pagenum > 1)
        {
            if (m_prevPageToken.isEmpty())
                SkipPagesBack();
            else
                GetLastResults();
        }
        else if (action == "PAGERIGHT" && m_searchResultList->GetCount() > 0 &&
                 m_pagenum + 10 < m_maxpage)
        {
            if (m_nextPageToken.isEmpty())
                SkipPagesForward();
            else
                GetMoreResults();
        }
        else if (action == "PREVVIEW" && m_pagenum > 1)
        {
            GetLastResults();
        }
        else if (action == "NEXTVIEW" && m_searchResultList->GetCount() > 0 &&
                 m_pagenum < m_maxpage)
        {
            GetMoreResults();
        }
        else
        {
            handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void NetSearch::ShowMenu(void)
{
    QString label = tr("Search Options");

    auto *menuPopup = new MythDialogBox(label, m_popupStack,
                                        "mythnetvisionmenupopup");

    if (menuPopup->Create())
    {
        m_popupStack->AddScreen(menuPopup);

        menuPopup->SetReturnEvent(this, "options");

        if (m_searchResultList->GetCount() > 0)
        {
            ResultItem *item = GetStreamItem();
            if (item)
            {
                if (item->GetDownloadable())
                    menuPopup->AddButton(tr("Stream Video"),
                                         &NetSearch::StreamWebVideo);
                menuPopup->AddButton(tr("Open Web Link"), &NetSearch::ShowWebVideo);

                QString filename = GetDownloadFilename(item->GetTitle(),
                                                       item->GetMediaURL());
                bool exists = false;

                if (filename.startsWith("myth://"))
                    exists = RemoteFile::Exists(filename);
                else
                    exists = QFile::exists(filename);

                if (item->GetDownloadable() &&
                    GetFocusWidget() == m_searchResultList)
                {
                    if (exists)
                    {
                        menuPopup->AddButton(tr("Play"),
                                             qOverload<>(&NetSearch::DoPlayVideo));
                    }
                    else
                    {
                        menuPopup->AddButton(tr("Save This Video"),
                                             &NetSearch::DoDownloadAndPlay);
                    }
                }

                if (item->GetDownloadable() &&
                    GetFocusWidget() == m_searchResultList &&
                    exists)
                {
                    menuPopup->AddButton(tr("Delete"), &NetSearch::SlotDeleteVideo);
                }
            }
        }

        if (m_pagenum > 1)
            menuPopup->AddButton(tr("Previous Page"), &NetSearch::GetLastResults);
        if (m_searchResultList->GetCount() > 0 && m_pagenum < m_maxpage)
            menuPopup->AddButton(tr("Next Page"), &NetSearch::GetMoreResults);
        if (m_pagenum > 1 && m_prevPageToken.isEmpty())
            menuPopup->AddButton(tr("Skip 10 Pages Back"),
                                 &NetSearch::SkipPagesBack);
        if (m_searchResultList->GetCount() > 0 && m_pagenum < m_maxpage &&
            m_nextPageToken.isEmpty())
            menuPopup->AddButton(tr("Skip 10 Pages Forward"),
                                 &NetSearch::SkipPagesForward);

        menuPopup->AddButton(tr("Manage Search Scripts"),
                             &NetSearch::RunSearchEditor);
    }
    else
    {
        delete menuPopup;
    }
}

void NetSearch::FillGrabberButtonList()
{
    m_siteList->Reset();

    for (const auto & g : std::as_const(m_grabberList))
    {
        auto *item = new MythUIButtonListItem(m_siteList, g->GetTitle());
        item->SetText(g->GetTitle(), "title");
        item->SetData(g->GetCommandline());
        QString thumb = QString("%1mythnetvision/icons/%2")
                            .arg(GetShareDir(), g->GetImage());
        item->SetImage(thumb);
    }
}

void NetSearch::DoSearch()
{
    m_searchResultList->Reset();

    int numScripts = m_siteList->GetCount();
    for (int i = 0; i < numScripts; i++)
        m_siteList->GetItemAt(i)->SetText("", "count");

    if (m_pageText)
        m_pageText->Reset();

    m_pagenum = 1;
    m_maxpage = 1;

    QString cmd = m_siteList->GetDataValue().toString();
    QString grabber = m_siteList->GetItemCurrent()->GetText();
    QString query = m_search->GetText();

    if (query.isEmpty())
        return;

    QFileInfo fi(cmd);
    m_currentCmd = fi.fileName();
    m_currentGrabber = m_siteList->GetCurrentPos();
    m_currentSearch = query;

    QString title = tr("Searching %1 for \"%2\"...")
                    .arg(grabber, query);
    OpenBusyPopup(title);

    if (!m_netSearch)
    {
        m_netSearch = new QNetworkAccessManager(this);
        connect(m_netSearch, &QNetworkAccessManager::finished,
                this, &NetSearch::SearchFinished);
    }

    QUrl init = GetMythXMLSearch(m_mythXML, m_currentSearch, m_currentCmd, "");
    QUrl req(init.toEncoded(), QUrl::TolerantMode);
    LOG(VB_GENERAL, LOG_INFO,
        QString("Using Search URL %1").arg(req.toString()));
    m_reply = m_netSearch->get(QNetworkRequest(req));
}

void NetSearch::GetLastResults()
{
    m_searchResultList->Reset();

    m_pagenum--;

    QString title = tr("Changing to page %1 of search \"%2\"...")
                    .arg(QString::number(m_pagenum), m_currentSearch);
    OpenBusyPopup(title);

    QString page = m_prevPageToken.isEmpty() ? QString::number(m_pagenum) :
        m_prevPageToken;

    QUrl req =
        GetMythXMLSearch(m_mythXML, m_currentSearch, m_currentCmd, page);
    m_reply = m_netSearch->get(QNetworkRequest(req));
}

void NetSearch::GetMoreResults()
{
    m_searchResultList->Reset();

    m_pagenum++;

    QString title = tr("Changing to page %1 of search \"%2\"...")
                    .arg(QString::number(m_pagenum), m_currentSearch);
    OpenBusyPopup(title);

    QString page = m_nextPageToken.isEmpty() ? QString::number(m_pagenum) :
        m_nextPageToken;

    QUrl req =
        GetMythXMLSearch(m_mythXML, m_currentSearch, m_currentCmd, page);
    m_reply = m_netSearch->get(QNetworkRequest(req));
}

void NetSearch::SkipPagesBack()
{
    if (m_pagenum < 11)
        m_pagenum = 2;
    else
        m_pagenum = m_pagenum - 10 + 1;

    GetLastResults();
}

void NetSearch::SkipPagesForward()
{
    m_pagenum = m_pagenum + 10 - 1;

    GetMoreResults();
}

void NetSearch::SearchFinished(void)
{
    CloseBusyPopup();

    auto *item = new Search();
    QByteArray data = m_reply->readAll();
    item->SetData(data);

    item->process();

    uint searchresults = item->numResults();
    uint returned = item->numReturned();
    uint firstitem = item->numIndex();

    m_nextPageToken = item->nextPageToken();
    m_prevPageToken = item->prevPageToken();

    if (returned == 0)
        return;

    m_siteList->GetItemAt(m_currentGrabber)->
        SetText(QString::number(searchresults), "count");

    if (firstitem + returned == searchresults)
        m_maxpage = m_pagenum;
    else
    {
        m_maxpage = searchresults / returned; // Whole pages
        if (searchresults % returned != 0)    // Partial page?
            m_maxpage++;
    }
    if (m_pageText && m_maxpage > 0 && m_pagenum > 0)
    {
        m_pageText->SetText(QString("%1 / %2")
                        .arg(QString::number(m_pagenum),
                             QString::number(m_maxpage)));
    }

    ResultItem::resultList list = item->GetVideoList();
    PopulateResultList(list);
    SetFocusWidget(m_searchResultList);
}

void NetSearch::SearchTimeout(Search * /*item*/)
{
    CloseBusyPopup();

    QString message = tr("Timed out waiting for query to "
                         "finish.  API might be down.");

    if (!m_okPopup)
    {
        m_okPopup = new MythConfirmationDialog(m_popupStack, message, false);

        if (m_okPopup->Create())
            m_popupStack->AddScreen(m_okPopup);
        else
        {
            delete m_okPopup;
            m_okPopup = nullptr;
        }
    }
}

void NetSearch::PopulateResultList(const ResultItem::resultList& list)
{
    for (const auto & result : std::as_const(list))
    {
        const QString& title = result->GetTitle();
        auto *item = new MythUIButtonListItem(m_searchResultList, title,
                                              QVariant::fromValue(result));
        InfoMap metadataMap;
        result->toMap(metadataMap);
        item->SetTextFromMap(metadataMap);

        if (!result->GetThumbnail().isEmpty())
        {
            QString dlfile = result->GetThumbnail();

            if (dlfile.contains("%SHAREDIR%"))
            {
                dlfile.replace("%SHAREDIR%", GetShareDir());
                item->SetImage(dlfile);
            }
            else
            {
                uint pos = m_searchResultList->GetItemPos(item);

                m_imageDownload->addThumb(result->GetTitle(),
                                          result->GetThumbnail(),
                                          QVariant::fromValue<uint>(pos));
            }
        }
    }
}

ResultItem* NetSearch::GetStreamItem()
{
    return m_searchResultList->GetDataValue().value<ResultItem*>();
}

void NetSearch::RunSearchEditor() const
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *searchedit = new SearchEditor(mainStack, "mythnetsearchedit");

    if (searchedit->Create())
    {
        connect(searchedit, &NetEditorBase::ItemsChanged,
                this, &NetSearch::DoListRefresh);

        mainStack->AddScreen(searchedit);
    }
    else
    {
        delete searchedit;
    }
}

void NetSearch::DoListRefresh()
{
    m_grabberList = findAllDBSearchGrabbers(VIDEO_FILE);
    if (m_grabberList.isEmpty())
        RunSearchEditor();

    LoadData();
}

void NetSearch::SlotItemChanged()
{
    auto *item = m_searchResultList->GetDataValue().value<ResultItem*>();

    if (item && GetFocusWidget() == m_searchResultList)
    {
        SetTextAndThumbnail(m_searchResultList->GetItemCurrent(), item);

        if (m_downloadable)
        {
            if (item->GetDownloadable())
                m_downloadable->DisplayState("yes");
            else
                m_downloadable->DisplayState("no");
        }
    }
    else if (GetFocusWidget() == m_siteList)
    {
        MythUIButtonListItem *btn = m_siteList->GetItemCurrent();
        std::shared_ptr<MythSortHelper>sh = getMythSortHelper();
        QString title = btn->GetText();

        ResultItem res(title, sh->doTitle(title), // title, sortTitle
                       QString(), QString(), // subtitle, sortSubtitle
                       QString(), // description
                       QString(), QString(), QString(), QString(),
                       QDateTime(), nullptr, nullptr, -1, QString(), QStringList(),
                       QString(), QStringList(), 0, 0, QString(),
                       false, QStringList(), 0, 0, false);

        SetTextAndThumbnail(btn, &res);
    }
}

void NetSearch::SetTextAndThumbnail(MythUIButtonListItem *btn, ResultItem *item)
{
    InfoMap metadataMap;
    item->toMap(metadataMap);
    SetTextFromMap(metadataMap);

    SetThumbnail(btn);
}

void NetSearch::SetThumbnail(MythUIButtonListItem *btn)
{
    if (m_thumbImage)
    {
        QString filename = btn->GetImageFilename();
        if (filename.contains("%SHAREDIR%"))
            filename.replace("%SHAREDIR%", GetShareDir());

        if (!filename.isEmpty())
        {
            m_thumbImage->SetFilename(filename);
            m_thumbImage->Load();
        }
        else
        {
            m_thumbImage->Reset();
        }
    }
}

void NetSearch::customEvent(QEvent *event)
{
    if (event->type() == ThumbnailDLEvent::kEventType)
    {
        auto *tde = dynamic_cast<ThumbnailDLEvent *>(event);
        if (tde == nullptr)
            return;

        ThumbnailData *data = tde->m_thumb;
        if (!data)
            return;

        QString title = data->title;
        QString file = data->url;
        uint pos = data->data.value<uint>();

        if (file.isEmpty() || !((uint)m_searchResultList->GetCount() >= pos))
            return;

        MythUIButtonListItem *item = m_searchResultList->GetItemAt(pos);

        if (!item)
            return;

        if (item->GetText() == title)
            item->SetImage(file);

        if (m_searchResultList->GetItemCurrent() == item)
            SetThumbnail(item);
    }
    else
    {
        NetBase::customEvent(event);
    }
}
