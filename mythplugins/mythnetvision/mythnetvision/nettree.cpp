// qt
#include <QFileInfo>
#include <QString>
#include <QtAlgorithms>

// MythTV
#include <libmythbase/mythcorecontext.h>
#include <libmythbase/mythdb.h>
#include <libmythbase/mythdirs.h>
#include <libmythbase/mythlogging.h>
#include <libmythbase/mythsorthelper.h>
#include <libmythbase/mythsystemlegacy.h>
#include <libmythbase/netgrabbermanager.h>
#include <libmythbase/netutils.h>
#include <libmythbase/rssmanager.h>
#include <libmythbase/rssparse.h>
#include <libmythmetadata/videoutils.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythprogressdialog.h>
#include <libmythui/mythscreenstack.h>
#include <libmythui/mythuiimage.h>
#include <libmythui/mythuitext.h>

// mythnetvision
#include "netcommon.h"
#include "nettree.h"
#include "rsseditor.h"
#include "treeeditor.h"

class GrabberScript;

const QString NetTree::kRSSNode = tr("RSS Feeds");
const QString NetTree::kSearchNode = tr("Searches");
const QString NetTree::kDownloadNode = tr("Downloaded Files");

namespace
{
    MythGenericTree *GetNodePtrFromButton(MythUIButtonListItem *item)
    {
        if (item)
            return item->GetData().value<MythGenericTree *>();

        return nullptr;
    }
}

NetTree::NetTree(DialogType type, MythScreenStack *parent, const char *name)
    : NetBase(parent, name),
      m_gdt(new GrabberDownloadThread(this)), m_type(type)
{
    connect(m_gdt, &GrabberDownloadThread::finished, this, &NetTree::DoTreeRefresh);
    m_updateFreq = gCoreContext->GetNumSetting(
                       "mythNetTree.updateFreq", 6);
    m_rssAutoUpdate = gCoreContext->GetBoolSetting(
                       "mythnetvision.rssBackgroundFetch", false);
    m_treeAutoUpdate = gCoreContext->GetBoolSetting(
                       "mythnetvision.backgroundFetch", false);
}

bool NetTree::Create()
{
    QString windowName = "gallery";

    switch (m_type)
    {
        case DLG_GALLERY:
            windowName = "gallery";
            break;
        case DLG_BROWSER:
            windowName = "browser";
            break;
        case DLG_TREE:
            windowName = "tree";
            break;
        case DLG_DEFAULT:
        default:
            break;
    }

    if (!LoadWindowFromXML("netvision-ui.xml", windowName, this))
        return false;

    bool err = false;
    if (m_type == DLG_TREE)
        UIUtilE::Assign(this, m_siteMap, "videos", &err);
    else
        UIUtilE::Assign(this, m_siteButtonList, "videos", &err);

    UIUtilW::Assign(this, m_noSites, "nosites");

    UIUtilW::Assign(this, m_thumbImage, "preview");

    UIUtilW::Assign(this, m_downloadable, "downloadable");

    m_siteGeneric = new MythGenericTree("site root", 0, false);
    m_currentNode = m_siteGeneric;

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen '" + windowName + "'");
        return false;
    }

    BuildFocusList();

    LoadInBackground();

    if (m_type == DLG_TREE)
    {
        SetFocusWidget(m_siteMap);

        connect(m_siteMap, &MythUIButtonTree::itemClicked,
                this, &NetTree::StreamWebVideo);
        connect(m_siteMap, &MythUIButtonTree::itemSelected,
                this, &NetTree::SlotItemChanged);
        connect(m_siteMap, &MythUIButtonTree::nodeChanged,
                this, &NetTree::SlotItemChanged);
    }
    else
    {
        SetFocusWidget(m_siteButtonList);

        connect(m_siteButtonList, &MythUIButtonList::itemClicked,
                this, &NetTree::HandleSelect);
        connect(m_siteButtonList, &MythUIButtonList::itemSelected,
                this, &NetTree::SlotItemChanged);
    }

    return true;
}

void NetTree::Load()
{
    m_grabberList = findAllDBTreeGrabbersByHost(VIDEO_FILE);
    m_rssList = findAllDBRSS();

    FillTree();
}

void NetTree::SetCurrentNode(MythGenericTree *node)
{
    if (!node)
        return;

    m_currentNode = node;
}

NetTree::~NetTree()
{
    delete m_siteGeneric;
    m_siteGeneric = nullptr;

    delete m_gdt;
    m_gdt = nullptr;

    m_rssList.clear();

    qDeleteAll(m_videos);
    m_videos.clear();
}

void NetTree::LoadData(void)
{
    if (m_type == DLG_TREE)
        m_siteMap->AssignTree(m_siteGeneric);
    else
    {
        m_siteButtonList->Reset();

        if (!m_currentNode)
        {
            SetCurrentNode(m_siteGeneric);
            return;
        }

        MythGenericTree *selectedNode = m_currentNode->getSelectedChild();

        using MGTreeChildList = QList<MythGenericTree *>;
        MGTreeChildList *lchildren = m_currentNode->getAllChildren();

        for (auto * child : std::as_const(*lchildren))
        {
            if (child != nullptr)
            {
                auto *item =
                        new MythUIButtonListItem(m_siteButtonList, QString(), nullptr,
                                true, MythUIButtonListItem::NotChecked);

                item->SetData(QVariant::fromValue(child));

                UpdateItem(item);

                if (child == selectedNode)
                    m_siteButtonList->SetItemCurrent(item);
            }
        }

        SlotItemChanged();
    }

    if (m_siteGeneric->childCount() == 0 && m_noSites)
        m_noSites->SetVisible(true);
    else if (m_noSites)
        m_noSites->SetVisible(false);

    if (m_siteGeneric->childCount() == 0)
        RunTreeEditor();
}

void NetTree::UpdateItem(MythUIButtonListItem *item)
{
    if (!item)
        return;

    MythGenericTree *node = GetNodePtrFromButton(item);

    if (!node)
        return;

    auto *site = node->GetData().value<RSSSite *>();
    auto *video = node->GetData().value<ResultItem *>();

    int nodeInt = node->getInt();

    if (nodeInt == kSubFolder)
    {
        item->SetText(QString("%1").arg(node->visibleChildCount()),
                      "childcount");
        item->DisplayState("subfolder", "nodetype");
        item->SetText(node->GetText(), "title");
        item->SetText(node->GetText());
        item->SetImage(node->GetData().toString());
    }
    else if (nodeInt == kUpFolder)
    {
        item->DisplayState("upfolder", "nodetype");
        item->SetText(node->GetText(), "title");
        item->SetText(node->GetText());
    }

    if (site)
    {
        item->SetText(site->GetTitle());
        item->SetText(site->GetDescription(), "description");
        item->SetText(site->GetURL(), "url");
        item->SetImage(site->GetImage());
    }
    else if (video)
    {
        item->SetText(video->GetTitle());

        InfoMap metadataMap;
        video->toMap(metadataMap);
        item->SetTextFromMap(metadataMap);

        int pos = 0;
        if (m_type != DLG_TREE)
            pos = m_siteButtonList->GetItemPos(item);

        QString dlfile = video->GetThumbnail();
        if (dlfile.contains("%SHAREDIR%"))
            dlfile.replace("%SHAREDIR%", GetShareDir());
        else
            dlfile = getDownloadFilename(video->GetTitle(),
                                         video->GetThumbnail());

        if (QFile::exists(dlfile))
            item->SetImage(dlfile);
        else if (m_imageDownload && video->GetThumbnail().startsWith("http"))
            m_imageDownload->addThumb(video->GetTitle(), video->GetThumbnail(),
                                      QVariant::fromValue<uint>(pos));
    }
    else
    {
        item->SetText(node->GetText());
        if (!node->GetData().toString().isEmpty())
        {
            QString tpath = node->GetData().toString();
            if (tpath.startsWith("http://"))
            {
                uint pos = 0;
                if (m_type != DLG_TREE)
                    pos = m_siteButtonList->GetItemPos(item);

                QString dlfile = GetThumbnailFilename(tpath,
                                                      node->GetText());
                if (QFile::exists(dlfile))
                    item->SetImage(dlfile);
                else if (m_imageDownload)
                    m_imageDownload->addThumb(node->GetText(), tpath,
                                              QVariant::fromValue<uint>(pos));
            }
            else if (tpath != "0")
            {
                QString filename = node->GetData().toString();
                if (filename.contains("%SHAREDIR%"))
                    filename.replace("%SHAREDIR%", GetShareDir());
                item->SetImage(filename);
            }
        }
    }
}

void NetTree::HandleSelect(MythUIButtonListItem *item)
{
    MythGenericTree *node = GetNodePtrFromButton(item);
    if (!node)
        return;

    int nodeInt = node->getInt();

    switch (nodeInt)
    {
        case kSubFolder:
            HandleDirSelect(node);
            break;
        case kUpFolder:
            GoBack();
            break;
        default:
        {
            StreamWebVideo();
        }
    }
    SlotItemChanged();
}

void NetTree::HandleDirSelect(MythGenericTree *node)
{
    if (m_imageDownload && m_imageDownload->isRunning())
        m_imageDownload->cancel();

    SetCurrentNode(node);
    LoadData();
}

bool NetTree::GoBack()
{
    bool handled = false;

    if (m_imageDownload && m_imageDownload->isRunning())
        m_imageDownload->cancel();

    if (m_currentNode != m_siteGeneric)
    {
        MythGenericTree *lparent = m_currentNode->getParent();
        if (lparent)
        {
            SetCurrentNode(lparent);
            handled = true;
        }
    }

    LoadData();

    return handled;
}

bool NetTree::keyPressEvent(QKeyEvent *event)
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
        {
            ShowMenu();
        }
        else if (action == "ESCAPE")
        {
            if (m_type != DLG_TREE
                    && !GetMythMainWindow()->IsExitingToMain()
                    && m_currentNode != m_siteGeneric)
                handled = GoBack();
            else
                handled = false;
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

void NetTree::ShowMenu(void)
{
    QString label = tr("Playback/Download Options");

    auto *menu = new MythMenu(label, this, "options");

    ResultItem *item = nullptr;
    if (m_type == DLG_TREE)
    {
        MythGenericTree *node = m_siteMap->GetCurrentNode();

        if (node)
            item = node->GetData().value<ResultItem *>();
    }
    else
    {
        MythGenericTree *node =
            GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (node)
            item = node->GetData().value<ResultItem *>();
    }

    if (item)
    {
        if (item->GetDownloadable())
            menu->AddItem(tr("Stream Video"), &NetTree::StreamWebVideo);
        menu->AddItem(tr("Open Web Link"), &NetTree::ShowWebVideo);

        if (item->GetDownloadable())
            menu->AddItem(tr("Save This Video"), &NetTree::DoDownloadAndPlay);
    }

    menu->AddItem(tr("Scan/Manage Subscriptions"), nullptr, CreateShowManageMenu());
    menu->AddItem(tr("Change View"), nullptr, CreateShowViewMenu());

    auto *menuPopup =
        new MythDialogBox(menu, m_popupStack, "mythnettreemenupopup");

    if (menuPopup->Create())
        m_popupStack->AddScreen(menuPopup);
    else
        delete menuPopup;
}

MythMenu* NetTree::CreateShowViewMenu()
{
    QString label = tr("View Options");

    auto *menu = new MythMenu(label, this, "options");

    if (m_type != DLG_TREE)
        menu->AddItem(tr("Switch to List View"), &NetTree::SwitchTreeView);
    if (m_type != DLG_GALLERY)
        menu->AddItem(tr("Switch to Gallery View"), &NetTree::SwitchGalleryView);
    if (m_type != DLG_BROWSER)
        menu->AddItem(tr("Switch to Browse View"), &NetTree::SwitchBrowseView);

    return menu;
}

MythMenu* NetTree::CreateShowManageMenu()
{
    QString label = tr("Subscription Management");

    auto *menu = new MythMenu(label, this, "options");

    menu->AddItem(tr("Update Site Maps"), &NetTree::UpdateTrees);
    menu->AddItem(tr("Update RSS"), &NetTree::UpdateRSS);
    menu->AddItem(tr("Manage Site Subscriptions"), &NetTree::RunTreeEditor);
    menu->AddItem(tr("Manage RSS Subscriptions"), &NetTree::RunRSSEditor);
    if (!m_treeAutoUpdate)
    {
        menu->AddItem(tr("Enable Automatic Site Updates"),
                      &NetTree::ToggleTreeUpdates);
    }
    else
    {
        menu->AddItem(tr("Disable Automatic Site Updates"),
                      &NetTree::ToggleTreeUpdates);
    }
//    if (!m_rssAutoUpdate)
//        menu->AddItem(tr("Enable Automatic RSS Updates"), &NetTree::ToggleRSSUpdates);
//    else
//        menu->AddItem(tr("Disable Automatic RSS Updates"), &NetTree::ToggleRSSUpdates);

    return menu;
}

void NetTree::SwitchTreeView()
{
    m_type = DLG_TREE;
    SwitchView();
}

void NetTree::SwitchGalleryView()
{
    m_type = DLG_GALLERY;
    SwitchView();
}

void NetTree::SwitchBrowseView()
{
    m_type = DLG_BROWSER;
    SwitchView();
}

void NetTree::SwitchView()
{
    auto *nettree =
        new NetTree(m_type, GetMythMainWindow()->GetMainStack(), "nettree");

    if (nettree->Create())
    {
        gCoreContext->SaveSetting("mythnetvision.ViewMode", m_type);
        MythScreenStack *screenStack = GetScreenStack();
        screenStack->AddScreen(nettree);
        screenStack->PopScreen(this, false, false);
        deleteLater();
    }
    else
    {
        delete nettree;
    }
}

void NetTree::FillTree()
{
    // First let's add all the RSS
    if (!m_rssList.isEmpty())
    {
        auto *rssGeneric = new MythGenericTree(kRSSNode, kSubFolder, false);

        // Add an upfolder
        if (m_type != DLG_TREE)
            rssGeneric->addNode(tr("Back"), kUpFolder, true, false);

        rssGeneric->SetData(QString("%1/mythnetvision/icons/rss.png")
                            .arg(GetShareDir()));

        for (const auto & feed : std::as_const(m_rssList))
        {
            ResultItem::resultList items = getRSSArticles(feed->GetTitle(),
                                                          VIDEO_PODCAST);
            auto *ret =
                new MythGenericTree(feed->GetTitle(), kSubFolder, false);
            ret->SetData(QVariant::fromValue(feed));
            rssGeneric->addNode(ret);

            // Add an upfolder
            if (m_type != DLG_TREE)
                ret->addNode(tr("Back"), kUpFolder, true, false);

            for (const auto & item : std::as_const(items))
                AddFileNode(ret, item);
            SetSubfolderData(ret);
        }

        m_siteGeneric->addNode(rssGeneric);
        SetSubfolderData(rssGeneric);
    }

    // Now let's add all the grabber trees
    for (const auto & g : std::as_const(m_grabberList))
    {

        QMultiMap<QPair<QString,QString>, ResultItem*> treePathsNodes =
                           getTreeArticles(g->GetTitle(), VIDEO_FILE);

        QList< QPair<QString,QString> > paths = treePathsNodes.uniqueKeys();

        auto *ret = new MythGenericTree(g->GetTitle(), kSubFolder, false);
        QString thumb = QString("%1mythnetvision/icons/%2")
            .arg(GetShareDir(), g->GetImage());
        ret->SetData(QVariant::fromValue(thumb));

        // Add an upfolder
        if (m_type != DLG_TREE)
            ret->addNode(tr("Back"), kUpFolder, true, false);

        for (const auto & path : std::as_const(paths))
        {
            QStringList curPaths = path.first.split("/");
            QString dirthumb = path.second;
            QList<ResultItem*> videos = treePathsNodes.values(path);
            BuildGenericTree(ret, curPaths, dirthumb, videos);
        }
        m_siteGeneric->addNode(ret);
        SetSubfolderData(ret);
    }
    m_siteGeneric->sortByString();
}

void NetTree::BuildGenericTree(MythGenericTree *dst, QStringList paths,
                               const QString& dirthumb, const QList<ResultItem*>& videos)
{
    MythGenericTree *folder = nullptr;

    // A little loop to determine what path of the provided path might
    // already exist in the tree.

    while (folder == nullptr && !paths.empty())
    {
        QString curPath = paths.takeFirst();
        curPath.replace("|", "/");
        MythGenericTree *tmp = dst->getChildByName(curPath);
        if (tmp)
            dst = tmp;
        else
            folder = new MythGenericTree(curPath, kSubFolder, false);
    }

    if (!folder)
       return;

    folder->SetData(dirthumb);
    dst->addNode(folder);

    // Add an upfolder
    if (m_type != DLG_TREE)
        folder->addNode(tr("Back"), kUpFolder, true, false);

    if (!paths.empty())
        BuildGenericTree(folder, paths, dirthumb, videos);
    else
    {
        // File Handling
        for (const auto & video : std::as_const(videos))
            AddFileNode(folder, video);
    }
    SetSubfolderData(folder);
}

void NetTree::AddFileNode(MythGenericTree *where_to_add, ResultItem *video)
{
    QString title = video->GetTitle();
    title.replace("&amp;", "&");
    MythGenericTree *sub_node = where_to_add->addNode(title, 0, true);
    sub_node->SetData(QVariant::fromValue(video));

    InfoMap textMap;
    video->toMap(textMap);
    sub_node->SetTextFromMap(textMap);

    m_videos.append(video);
}

ResultItem* NetTree::GetStreamItem()
{
    ResultItem *item = nullptr;

    if (m_type == DLG_TREE)
        item = m_siteMap->GetCurrentNode()->GetData().value<ResultItem *>();
    else
    {
        MythGenericTree *node =
            GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (node)
            item = node->GetData().value<ResultItem *>();
    }
    return item;
}

void NetTree::UpdateResultItem(ResultItem *item)
{
    InfoMap metadataMap;
    item->toMap(metadataMap);
    SetTextFromMap(metadataMap);

    if (!item->GetThumbnail().isEmpty() && m_thumbImage)
    {
        m_thumbImage->Reset();
        QString dlfile = item->GetThumbnail();
        if (dlfile.contains("%SHAREDIR%"))
        {
            dlfile.replace("%SHAREDIR%", GetShareDir());
            m_thumbImage->SetFilename(dlfile);
            m_thumbImage->Load();
        }
        else
        {
            QString sFilename = getDownloadFilename(item->GetTitle(),
                                                    item->GetThumbnail());

            bool exists = QFile::exists(sFilename);
            if (exists)
            {
                m_thumbImage->SetFilename(sFilename);
                m_thumbImage->Load();
            }
            else if (item->GetThumbnail().startsWith("http"))
            {
                m_imageDownload->addThumb(item->GetTitle(),
                                          item->GetThumbnail(),
                                          QVariant::fromValue<uint>(0));
            }
        }
    }
    else if (m_thumbImage)
    {
        m_thumbImage->Reset();
    }

    if (m_downloadable)
    {
        if (item->GetDownloadable())
            m_downloadable->DisplayState("yes");
        else
            m_downloadable->DisplayState("no");
    }
}

void NetTree::UpdateSiteItem(RSSSite *site)
{
    ResultItem res =
        ResultItem(site->GetTitle(), site->GetSortTitle(),
                   QString(), QString(), // no subtitle information
                   site->GetDescription(),
                   site->GetURL(), site->GetImage(), QString(),
                   site->GetAuthor(), QDateTime(), nullptr, nullptr, -1, QString(),
                   QStringList(), QString(), QStringList(), 0, 0, QString(),
                   false, QStringList(), 0, 0, false);

    InfoMap metadataMap;
    res.toMap(metadataMap);
    SetTextFromMap(metadataMap);

    if (!site->GetImage().isEmpty() && m_thumbImage)
    {
        m_thumbImage->SetFilename(site->GetImage());
        m_thumbImage->Load();
    }
    else if (m_thumbImage)
    {
        m_thumbImage->Reset();
    }

    if (m_downloadable)
        m_downloadable->Reset();
}

void NetTree::UpdateCurrentItem(void)
{
    QString title;

    if (m_type == DLG_TREE)
        title = m_siteMap->GetItemCurrent()->GetText();
    else
        title = m_siteButtonList->GetItemCurrent()->GetText();

    QString thumb;
    if (m_type == DLG_TREE)
        thumb = m_siteMap->GetCurrentNode()->GetData().toString();
    else
    {
        MythGenericTree *node =
            GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (node)
            thumb = node->GetData().toString();
    }

    std::shared_ptr<MythSortHelper>sh = getMythSortHelper();
    ResultItem res =
        ResultItem(title, sh->doTitle(title), // title, sortTitle
                   QString(), QString(), // no subtitle information
                   QString(), // description
                   QString(), thumb, QString(),
                   QString(), QDateTime(), nullptr, nullptr, -1, QString(),
                   QStringList(), QString(), QStringList(), 0, 0, QString(),
                   false, QStringList(), 0, 0, false);

    InfoMap metadataMap;
    res.toMap(metadataMap);
    SetTextFromMap(metadataMap);

    if (m_thumbImage)
    {
        if (!thumb.startsWith("http://"))
        {
            if (thumb.contains("%SHAREDIR%"))
                thumb.replace("%SHAREDIR%", GetShareDir());

            bool exists = QFile::exists(thumb);

            if (exists)
            {
                m_thumbImage->SetFilename(thumb);
                m_thumbImage->Load();
            }
            else
            {
                m_thumbImage->Reset();
            }
        }
        else
        {
            const QString& url = thumb;
            QString title2;
            if (m_type == DLG_TREE)
                title2 = m_siteMap->GetItemCurrent()->GetText();
            else
                title2 = m_siteButtonList->GetItemCurrent()->GetText();

            QString sFilename = GetDownloadFilename(title2, url);

            bool exists = QFile::exists(sFilename);
            if (exists && !url.isEmpty())
            {
                m_thumbImage->SetFilename(sFilename);
                m_thumbImage->Load();
            }
            else
            {
                m_thumbImage->Reset();
            }
        }
    }

    if (m_downloadable)
        m_downloadable->Reset();
}

void NetTree::SlotItemChanged()
{
    ResultItem *item = nullptr;
    RSSSite *site = nullptr;

    if (m_type == DLG_TREE)
    {
        item = m_siteMap->GetCurrentNode()->GetData().value<ResultItem *>();
        site = m_siteMap->GetCurrentNode()->GetData().value<RSSSite *>();
    }
    else
    {
        MythGenericTree *node =
            GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (!node)
            return;

        item = node->GetData().value<ResultItem *>();
        site = node->GetData().value<RSSSite *>();
    }

    if (item)
        UpdateResultItem(item);
    else if (site)
        UpdateSiteItem(site);
    else
        UpdateCurrentItem();
}

void NetTree::RunTreeEditor() const
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *treeedit = new TreeEditor(mainStack, "mythnettreeedit");

    if (treeedit->Create())
    {
        connect(treeedit, &NetEditorBase::ItemsChanged, this, &NetTree::DoTreeRefresh);

        mainStack->AddScreen(treeedit);
    }
    else
    {
        delete treeedit;
    }
}

void NetTree::RunRSSEditor() const
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *rssedit = new RSSEditor(mainStack, "mythnetrssedit");

    if (rssedit->Create())
    {
        connect(rssedit, &RSSEditor::ItemsChanged, this, &NetTree::UpdateRSS);

        mainStack->AddScreen(rssedit);
    }
    else
    {
        delete rssedit;
    }
}

void NetTree::DoTreeRefresh()
{
    CloseBusyPopup();

    TreeRefresh();
}

void NetTree::TreeRefresh()
{
    delete m_siteGeneric;
    m_siteGeneric = new MythGenericTree("site root", 0, false);
    m_currentNode = m_siteGeneric;

    m_grabberList = findAllDBTreeGrabbers();
    m_rssList = findAllDBRSS();

    FillTree();
    LoadData();
    SwitchView();
}

void NetTree::UpdateRSS()
{
    if (findAllDBRSS().isEmpty())
        return;

    QString title(tr("Updating RSS.  This could take a while..."));
    OpenBusyPopup(title);

    auto *rssMan = new RSSManager();
    connect(rssMan, &RSSManager::finished, this, &NetTree::DoTreeRefresh);
    rssMan->startTimer();
    rssMan->doUpdate();
}

void NetTree::UpdateTrees()
{
    if (m_grabberList.count() == 0)
        return;

    QString title(tr("Updating Site Maps.  This could take a while..."));
    OpenBusyPopup(title);
    m_gdt->refreshAll();
}

void NetTree::ToggleRSSUpdates()
{
    m_rssAutoUpdate = !m_rssAutoUpdate;
    gCoreContext->SaveBoolSetting("mythnetvision.rssBackgroundFetch",
                                  m_rssAutoUpdate);
}

void NetTree::ToggleTreeUpdates()
{
    m_treeAutoUpdate = !m_treeAutoUpdate;
    gCoreContext->SaveBoolSetting("mythnetvision.backgroundFetch",
                                  m_treeAutoUpdate);
}

void NetTree::customEvent(QEvent *event)
{
    if (event->type() == ThumbnailDLEvent::kEventType)
    {
        auto *tde = dynamic_cast<ThumbnailDLEvent *>(event);
        if (!tde)
            return;

        ThumbnailData *data = tde->m_thumb;
        if (!data)
            return;

        QString title = data->title;
        QString file = data->url;
        uint pos = data->data.value<uint>();

        if (file.isEmpty())
            return;

        if (m_type == DLG_TREE)
        {
            if (title == m_siteMap->GetCurrentNode()->GetText() &&
                m_thumbImage)
            {
                m_thumbImage->SetFilename(file);
                m_thumbImage->Load();
                m_thumbImage->Show();
            }
        }
        else
        {
            if (!((uint)m_siteButtonList->GetCount() >= pos))
                return;

            MythUIButtonListItem *item = m_siteButtonList->GetItemAt(pos);

            if (item && item->GetText() == title)
                item->SetImage(file);
        }
    }
    else
    {
        NetBase::customEvent(event);
    }
}

void NetTree::SetSubfolderData(MythGenericTree *folder)
{
    folder->SetText(QString("%1").arg(folder->visibleChildCount()),
                    "childcount");
    folder->DisplayState("subfolder", "nodetype");
}
