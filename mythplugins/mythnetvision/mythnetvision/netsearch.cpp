// qt
#include <QString>
#include <QMetaType>
#include <QDateTime>
#include <QFileInfo>
#include <QImageReader>

// myth
#include <mythdb.h>
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythdirs.h>
#include <mythsystemlegacy.h>
#include <mythprogressdialog.h>
#include <storagegroup.h>
#include <remotefile.h>
#include <netgrabbermanager.h>
#include <netutils.h>
#include <metadata/metadataimagedownload.h>
#include <metadata/videoutils.h>
#include <rssparse.h>
#include <mythcoreutil.h>

#include "netsearch.h"
#include "netcommon.h"
#include "rsseditor.h"
#include "searcheditor.h"

using namespace std;

// ---------------------------------------------------

NetSearch::NetSearch(MythScreenStack *parent, const char *name)
    : NetBase(parent, name),
      m_searchResultList(NULL),      m_siteList(NULL),
      m_search(NULL),                m_pageText(NULL),
      m_noSites(NULL),               m_progress(NULL),
      m_okPopup(NULL),               m_netSearch(NULL),
      m_reply(NULL),
      m_currentSearch(QString()),    m_currentGrabber(0),
      m_currentCmd(QString()),       m_pagenum(0),
      m_maxpage(0),                  m_mythXML(GetMythXMLURL())
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
    connect(m_siteList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            SLOT(SlotItemChanged()));
    connect(m_siteList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(DoSearch(void)));
    connect(m_searchResultList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(StreamWebVideo(void)));
    connect(m_searchResultList, SIGNAL(itemSelected(MythUIButtonListItem *)),
            SLOT(SlotItemChanged()));

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
        m_netSearch = NULL;
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
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
            ShowMenu();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void NetSearch::ShowMenu(void)
{
    QString label = tr("Search Options");

    MythDialogBox *menuPopup = new MythDialogBox(label, m_popupStack,
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
                                         SLOT(StreamWebVideo()));
                menuPopup->AddButton(tr("Open Web Link"), SLOT(ShowWebVideo()));

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
                        menuPopup->AddButton(tr("Play"),
                                             SLOT(DoPlayVideo(filename)));
                    else
                        menuPopup->AddButton(tr("Save This Video"),
                                             SLOT(DoDownloadAndPlay()));
                }

                if (item->GetDownloadable() &&
                    GetFocusWidget() == m_searchResultList &&
                    exists)
                {
                    menuPopup->AddButton(tr("Delete"), SLOT(SlotDeleteVideo()));
                }
            }

            if (m_pagenum > 1)
                menuPopup->AddButton(tr("Previous Page"),
                                     SLOT(GetLastResults()));
            if (m_searchResultList->GetCount() > 0 && m_pagenum < m_maxpage)
                menuPopup->AddButton(tr("Next Page"), SLOT(GetMoreResults()));
        }
        menuPopup->AddButton(tr("Manage Search Scripts"),
                             SLOT(RunSearchEditor()));
    }
    else
        delete menuPopup;
}

void NetSearch::FillGrabberButtonList()
{
    m_siteList->Reset();

    for (GrabberScript::scriptList::iterator i = m_grabberList.begin();
            i != m_grabberList.end(); ++i)
    {
        MythUIButtonListItem *item =
                    new MythUIButtonListItem(m_siteList, (*i)->GetTitle());
        item->SetText((*i)->GetTitle(), "title");
        item->SetData((*i)->GetCommandline());
        QString thumb = QString("%1mythnetvision/icons/%2").arg(GetShareDir())
                            .arg((*i)->GetImage());
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
                    .arg(grabber).arg(query);
    OpenBusyPopup(title);

    if (!m_netSearch)
        m_netSearch = new QNetworkAccessManager(this);
    connect(m_netSearch, SIGNAL(finished(QNetworkReply*)),
            SLOT(SearchFinished(void)));

    QUrl init = GetMythXMLSearch(m_mythXML, m_currentSearch,
                                 m_currentCmd, m_pagenum);
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
                    .arg(QString::number(m_pagenum))
                    .arg(m_currentSearch);
    OpenBusyPopup(title);

    QUrl req = GetMythXMLSearch(m_mythXML, m_currentSearch,
                                m_currentCmd, m_pagenum);
    m_reply = m_netSearch->get(QNetworkRequest(req));
}

void NetSearch::GetMoreResults()
{
    m_searchResultList->Reset();

    m_pagenum++;

    QString title = tr("Changing to page %1 of search \"%2\"...")
                    .arg(QString::number(m_pagenum))
                    .arg(m_currentSearch);
    OpenBusyPopup(title);

    QUrl req = GetMythXMLSearch(m_mythXML, m_currentSearch,
                                m_currentCmd, m_pagenum);
    m_reply = m_netSearch->get(QNetworkRequest(req));
}

void NetSearch::SearchFinished(void)
{
    CloseBusyPopup();

    Search *item = new Search();
    QByteArray data = m_reply->readAll();
    item->SetData(data);

    item->process();

    uint searchresults = item->numResults();
    uint returned = item->numReturned();
    uint firstitem = item->numIndex();

    if (returned > 0)
        m_siteList->GetItemAt(m_currentGrabber)->
                  SetText(QString::number(
                  searchresults), "count");
    else
        return;

    if (firstitem + returned == searchresults)
        m_maxpage = m_pagenum;
    else
    {
        if (((float)searchresults/returned + 0.999) >=
            ((int)searchresults/returned + 1))
            m_maxpage = (searchresults/returned + 1);
        else
            m_maxpage = (searchresults/returned);
    }
    if (m_pageText && m_maxpage > 0 && m_pagenum > 0 && returned > 0)
        m_pageText->SetText(QString("%1 / %2")
                        .arg(QString::number(m_pagenum))
                        .arg(QString::number(m_maxpage)));

    ResultItem::resultList list = item->GetVideoList();
    PopulateResultList(list);
    SetFocusWidget(m_searchResultList);
}

void NetSearch::SearchTimeout(Search *)
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
            m_okPopup = NULL;
        }
    }
}

void NetSearch::PopulateResultList(ResultItem::resultList list)
{
    for (ResultItem::resultList::iterator i = list.begin();
            i != list.end(); ++i)
    {
        QString title = (*i)->GetTitle();
        MythUIButtonListItem *item =
            new MythUIButtonListItem(m_searchResultList, title,
                                     qVariantFromValue(*i));
        InfoMap metadataMap;
        (*i)->toMap(metadataMap);
        item->SetTextFromMap(metadataMap);

        if (!(*i)->GetThumbnail().isEmpty())
        {
            QString dlfile = (*i)->GetThumbnail();

            if (dlfile.contains("%SHAREDIR%"))
            {
                dlfile.replace("%SHAREDIR%", GetShareDir());
                item->SetImage(dlfile);
            }
            else
            {
                uint pos = m_searchResultList->GetItemPos(item);

                m_imageDownload->addThumb((*i)->GetTitle(),
                                          (*i)->GetThumbnail(),
                                          qVariantFromValue<uint>(pos));
            }
        }
    }
}

ResultItem* NetSearch::GetStreamItem()
{
    return qVariantValue<ResultItem *>(m_searchResultList->GetDataValue());
}

void NetSearch::RunSearchEditor()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    SearchEditor *searchedit = new SearchEditor(mainStack, "mythnetsearchedit");

    if (searchedit->Create())
    {
        connect(searchedit, SIGNAL(ItemsChanged()),
                this, SLOT(DoListRefresh()));

        mainStack->AddScreen(searchedit);
    }
    else
        delete searchedit;
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
    ResultItem *item =
              qVariantValue<ResultItem *>(m_searchResultList->GetDataValue());

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

        ResultItem res(btn->GetText(), QString(), QString(),
                       QString(), QString(), QString(), QString(),
                       QDateTime(), 0, 0, -1, QString(), QStringList(),
                       QString(), QStringList(), 0, 0, QString(),
                       0, QStringList(), 0, 0, 0);

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
            m_thumbImage->Reset();
    }
}

void NetSearch::customEvent(QEvent *event)
{
    if (event->type() == ThumbnailDLEvent::kEventType)
    {
        ThumbnailDLEvent *tde = (ThumbnailDLEvent *)event;
        ThumbnailData *data = tde->thumb;

        if (!data)
            return;

        QString title = data->title;
        QString file = data->url;
        uint pos = qVariantValue<uint>(data->data);

        if (file.isEmpty() || !((uint)m_searchResultList->GetCount() >= pos))
            return;

        MythUIButtonListItem *item = m_searchResultList->GetItemAt(pos);

        if (item && item->GetText() == title)
            item->SetImage(file);

        if (m_searchResultList->GetItemCurrent() == item)
            SetThumbnail(item);
    }
    else
        NetBase::customEvent(event);
}
