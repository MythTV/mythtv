// qt
#include <QString>
#include <QFileInfo>
#include <QtAlgorithms>

// myth
#include <mythdb.h>
#include <mythcontext.h>
#include <mythdirs.h>
#include <mythsystemlegacy.h>
#include <mythprogressdialog.h>
#include <rssparse.h>
#include <netutils.h>
#include <mythrssmanager.h>
#include <netgrabbermanager.h>
#include <mythcoreutil.h>
#include <metadata/videoutils.h>

// mythnetvision
#include "treeeditor.h"
#include "nettree.h"
#include "rsseditor.h"
#include "netcommon.h"

class GrabberScript;

const QString NetTree::RSSNode = tr("RSS Feeds");
const QString NetTree::SearchNode = tr("Searches");
const QString NetTree::DownloadNode = tr("Downloaded Files");

namespace
{
    MythGenericTree *GetNodePtrFromButton(MythUIButtonListItem *item)
    {
        if (item)
            return item->GetData().value<MythGenericTree *>();

        return NULL;
    }
}

NetTree::NetTree(DialogType type, MythScreenStack *parent, const char *name)
    : NetBase(parent, name),
      m_siteMap(NULL),               m_siteButtonList(NULL),
      m_siteGeneric(NULL),           m_currentNode(NULL),
      m_noSites(NULL),
      m_gdt(new GrabberDownloadThread(this)), m_type(type)
{
    connect(m_gdt, SIGNAL(finished()), SLOT(DoTreeRefresh()));
    m_updateFreq = gCoreContext->GetNumSetting(
                       "mythNetTree.updateFreq", 6);
    m_rssAutoUpdate = gCoreContext->GetNumSetting(
                       "mythnetvision.rssBackgroundFetch", 0);
    m_treeAutoUpdate = gCoreContext->GetNumSetting(
                       "mythnetvision.backgroundFetch", 0);
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

        connect(m_siteMap, SIGNAL(itemClicked(MythUIButtonListItem *)),
                SLOT(StreamWebVideo(void)));
        connect(m_siteMap, SIGNAL(itemSelected(MythUIButtonListItem *)),
                SLOT(SlotItemChanged(void)));
        connect(m_siteMap, SIGNAL(nodeChanged(MythGenericTree *)),
                SLOT(SlotItemChanged(void)));
    }
    else
    {
        SetFocusWidget(m_siteButtonList);

        connect(m_siteButtonList, SIGNAL(itemClicked(MythUIButtonListItem *)),
                SLOT(HandleSelect(MythUIButtonListItem *)));
        connect(m_siteButtonList, SIGNAL(itemSelected(MythUIButtonListItem *)),
                SLOT(SlotItemChanged(void)));
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
    m_siteGeneric = NULL;

    delete m_gdt;
    m_gdt = NULL;

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
            SetCurrentNode(m_siteGeneric);

        if (!m_currentNode)
            return;

        MythGenericTree *selectedNode = m_currentNode->getSelectedChild();

        typedef QList<MythGenericTree *> MGTreeChildList;
        MGTreeChildList *lchildren = m_currentNode->getAllChildren();

        for (MGTreeChildList::const_iterator p = lchildren->begin();
                p != lchildren->end(); ++p)
        {
            if (*p != NULL)
            {
                MythUIButtonListItem *item =
                        new MythUIButtonListItem(m_siteButtonList, QString(), 0,
                                true, MythUIButtonListItem::NotChecked);

                item->SetData(qVariantFromValue(*p));

                UpdateItem(item);

                if (*p == selectedNode)
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

    RSSSite *site = qVariantValue<RSSSite *>(node->GetData());
    ResultItem *video = qVariantValue<ResultItem *>(node->GetData());

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

        int pos;
        if (m_type == DLG_TREE)
            pos = 0;
        else
            pos = m_siteButtonList->GetItemPos(item);

        QString dlfile = video->GetThumbnail();
        if (dlfile.contains("%SHAREDIR%"))
            dlfile.replace("%SHAREDIR%", GetShareDir());
        else
            dlfile = getDownloadFilename(video->GetTitle(),
                                         video->GetThumbnail());

        if (QFile::exists(dlfile))
            item->SetImage(dlfile);
        else if (video->GetThumbnail().startsWith("http"))
            m_imageDownload->addThumb(video->GetTitle(), video->GetThumbnail(),
                                      qVariantFromValue<uint>(pos));
    }
    else
    {
        item->SetText(node->GetText());
        if (!node->GetData().toString().isEmpty())
        {
            QString tpath = node->GetData().toString();
            if (tpath.startsWith("http://"))
            {
                uint pos;
                if (m_type == DLG_TREE)
                    pos = 0;
                else
                    pos = m_siteButtonList->GetItemPos(item);

                QString dlfile = GetThumbnailFilename(tpath,
                                                      node->GetText());
                if (QFile::exists(dlfile))
                    item->SetImage(dlfile);
                else
                    m_imageDownload->addThumb(node->GetText(), tpath,
                                              qVariantFromValue<uint>(pos));
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
        QString action = actions[i];
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
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void NetTree::ShowMenu(void)
{
    QString label = tr("Playback/Download Options");

    MythMenu *menu = new MythMenu(label, this, "options");

    ResultItem *item = NULL;
    if (m_type == DLG_TREE)
    {
        MythGenericTree *node = m_siteMap->GetCurrentNode();

        if (node)
            item = qVariantValue<ResultItem *>(node->GetData());
    }
    else
    {
        MythGenericTree *node =
            GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (node)
            item = qVariantValue<ResultItem *>(node->GetData());
    }

    if (item)
    {
        if (item->GetDownloadable())
            menu->AddItem(tr("Stream Video"), SLOT(StreamWebVideo()));
        menu->AddItem(tr("Open Web Link"), SLOT(ShowWebVideo()));

        if (item->GetDownloadable())
            menu->AddItem(tr("Save This Video"), SLOT(DoDownloadAndPlay()));
    }

    menu->AddItem(tr("Scan/Manage Subscriptions"), NULL, CreateShowManageMenu());
    menu->AddItem(tr("Change View"), NULL, CreateShowViewMenu());

    MythDialogBox *menuPopup =
        new MythDialogBox(menu, m_popupStack, "mythnettreemenupopup");

    if (menuPopup->Create())
        m_popupStack->AddScreen(menuPopup);
    else
        delete menuPopup;
}

MythMenu* NetTree::CreateShowViewMenu()
{
    QString label = tr("View Options");

    MythMenu *menu = new MythMenu(label, this, "options");

    if (m_type != DLG_TREE)
        menu->AddItem(tr("Switch to List View"), SLOT(SwitchTreeView()));
    if (m_type != DLG_GALLERY)
        menu->AddItem(tr("Switch to Gallery View"), SLOT(SwitchGalleryView()));
    if (m_type != DLG_BROWSER)
        menu->AddItem(tr("Switch to Browse View"), SLOT(SwitchBrowseView()));

    return menu;
}

MythMenu* NetTree::CreateShowManageMenu()
{
    QString label = tr("Subscription Management");

    MythMenu *menu = new MythMenu(label, this, "options");

    menu->AddItem(tr("Update Site Maps"), SLOT(UpdateTrees()));
    menu->AddItem(tr("Update RSS"), SLOT(UpdateRSS()));
    menu->AddItem(tr("Manage Site Subscriptions"), SLOT(RunTreeEditor()));
    menu->AddItem(tr("Manage RSS Subscriptions"), SLOT(RunRSSEditor()));
    if (!m_treeAutoUpdate)
        menu->AddItem(tr("Enable Automatic Site Updates"),
                      SLOT(ToggleTreeUpdates()));
    else
        menu->AddItem(tr("Disable Automatic Site Updates"),
                      SLOT(ToggleTreeUpdates()));
//    if (!m_rssAutoUpdate)
//        menu->AddItem(tr("Enable Automatic RSS Updates"), SLOT(ToggleRSSUpdates()));
//    else
//        menu->AddItem(tr("Disable Automatic RSS Updates"), SLOT(ToggleRSSUpdates()));

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
    NetTree *nettree =
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
        delete nettree;
}

void NetTree::FillTree()
{
    // First let's add all the RSS
    if (!m_rssList.isEmpty())
    {
        MythGenericTree *rssGeneric =
            new MythGenericTree(RSSNode, kSubFolder, false);

        // Add an upfolder
        if (m_type != DLG_TREE)
            rssGeneric->addNode(tr("Back"), kUpFolder, true, false);

        rssGeneric->SetData(QString("%1/mythnetvision/icons/rss.png")
                            .arg(GetShareDir()));

        RSSSite::rssList::iterator i = m_rssList.begin();
        for (; i != m_rssList.end(); ++i)
        {
            ResultItem::resultList items = getRSSArticles((*i)->GetTitle(),
                                                          VIDEO_PODCAST);
            MythGenericTree *ret =
                new MythGenericTree((*i)->GetTitle(), kSubFolder, false);
            ret->SetData(qVariantFromValue(*i));
            rssGeneric->addNode(ret);

            // Add an upfolder
            if (m_type != DLG_TREE)
                ret->addNode(tr("Back"), kUpFolder, true, false);

            ResultItem::resultList::iterator it = items.begin();
            for (; it != items.end(); ++it)
                AddFileNode(ret, *it);
            SetSubfolderData(ret);
        }

        m_siteGeneric->addNode(rssGeneric);
        SetSubfolderData(rssGeneric);
    }

    // Now let's add all the grabber trees
    for (GrabberScript::scriptList::iterator i = m_grabberList.begin();
            i != m_grabberList.end(); ++i)
    {

        QMultiMap<QPair<QString,QString>, ResultItem*> treePathsNodes =
                           getTreeArticles((*i)->GetTitle(), VIDEO_FILE);

        QList< QPair<QString,QString> > paths = treePathsNodes.uniqueKeys();

        MythGenericTree *ret = new MythGenericTree(
                   (*i)->GetTitle(), kSubFolder, false);
        QString thumb = QString("%1mythnetvision/icons/%2").arg(GetShareDir())
                            .arg((*i)->GetImage());
        ret->SetData(qVariantFromValue(thumb));

        // Add an upfolder
        if (m_type != DLG_TREE)
            ret->addNode(tr("Back"), kUpFolder, true, false);

        for (QList<QPair<QString, QString> >::iterator i = paths.begin();
                i != paths.end(); ++i)
        {
            QStringList curPaths = (*i).first.split("/");
            QString dirthumb = (*i).second;
            QList<ResultItem*> videos = treePathsNodes.values(*i);
            BuildGenericTree(ret, curPaths, dirthumb, videos);
        }
        m_siteGeneric->addNode(ret);
        SetSubfolderData(ret);
    }
}

void NetTree::BuildGenericTree(MythGenericTree *dst, QStringList paths,
                               QString dirthumb, QList<ResultItem*> videos)
{
    MythGenericTree *folder = NULL;

    // A little loop to determine what path of the provided path might
    // already exist in the tree.

    while (folder == NULL && paths.size())
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

    if (paths.size())
        BuildGenericTree(folder, paths, dirthumb, videos);
    else
    {
        // File Handling
        for (QList<ResultItem*>::iterator it = videos.begin();
                it != videos.end(); ++it)
            AddFileNode(folder, *it);
    }
    SetSubfolderData(folder);
}

void NetTree::AddFileNode(MythGenericTree *where_to_add, ResultItem *video)
{
    QString title = video->GetTitle();
    title.replace("&amp;", "&");
    MythGenericTree *sub_node = where_to_add->addNode(title, 0, true);
    sub_node->SetData(qVariantFromValue(video));

    InfoMap textMap;
    video->toMap(textMap);
    sub_node->SetTextFromMap(textMap);

    m_videos.append(video);
}

ResultItem* NetTree::GetStreamItem()
{
    ResultItem *item = NULL;

    if (m_type == DLG_TREE)
        item =
            qVariantValue<ResultItem *>(m_siteMap->GetCurrentNode()->GetData());
    else
    {
        MythGenericTree *node =
            GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (node)
            item = qVariantValue<ResultItem *>(node->GetData());
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
                                          qVariantFromValue<uint>(0));
            }
        }
    }
    else if (m_thumbImage)
        m_thumbImage->Reset();

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
        ResultItem(site->GetTitle(), QString(), site->GetDescription(),
                   site->GetURL(), site->GetImage(), QString(),
                   site->GetAuthor(), QDateTime(), 0, 0, -1, QString(),
                   QStringList(), QString(), QStringList(), 0, 0, QString(),
                   0, QStringList(), 0, 0, 0);

    InfoMap metadataMap;
    res.toMap(metadataMap);
    SetTextFromMap(metadataMap);

    if (!site->GetImage().isEmpty() && m_thumbImage)
    {
        m_thumbImage->SetFilename(site->GetImage());
        m_thumbImage->Load();
    }
    else if (m_thumbImage)
        m_thumbImage->Reset();

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

    ResultItem res =
        ResultItem(title, QString(), QString(), QString(), thumb, QString(),
                   QString(), QDateTime(), 0, 0, -1, QString(),
                   QStringList(), QString(), QStringList(), 0, 0, QString(),
                   0, QStringList(), 0, 0, 0);

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
                m_thumbImage->Reset();
        }
        else
        {
            QString url = thumb;
            QString title;
            if (m_type == DLG_TREE)
                title = m_siteMap->GetItemCurrent()->GetText();
            else
                title = m_siteButtonList->GetItemCurrent()->GetText();

            QString sFilename = GetDownloadFilename(title, url);

            bool exists = QFile::exists(sFilename);
            if (exists && !url.isEmpty())
            {
                m_thumbImage->SetFilename(sFilename);
                m_thumbImage->Load();
            }
            else
                m_thumbImage->Reset();
        }
    }

    if (m_downloadable)
        m_downloadable->Reset();
}

void NetTree::SlotItemChanged()
{
    ResultItem *item;
    RSSSite *site;

    if (m_type == DLG_TREE)
    {
        item =
            qVariantValue<ResultItem *>(m_siteMap->GetCurrentNode()->GetData());
        site = qVariantValue<RSSSite *>(m_siteMap->GetCurrentNode()->GetData());
    }
    else
    {
        MythGenericTree *node =
            GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (!node)
            return;

        item = qVariantValue<ResultItem *>(node->GetData());
        site = qVariantValue<RSSSite *>(node->GetData());
    }

    if (item)
        UpdateResultItem(item);
    else if (site)
        UpdateSiteItem(site);
    else
        UpdateCurrentItem();
}

void NetTree::RunTreeEditor()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    TreeEditor *treeedit = new TreeEditor(mainStack, "mythnettreeedit");

    if (treeedit->Create())
    {
        connect(treeedit, SIGNAL(ItemsChanged()), this, SLOT(DoTreeRefresh()));

        mainStack->AddScreen(treeedit);
    }
    else
        delete treeedit;
}

void NetTree::RunRSSEditor()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    RSSEditor *rssedit = new RSSEditor(mainStack, "mythnetrssedit");

    if (rssedit->Create())
    {
        connect(rssedit, SIGNAL(ItemsChanged()), this, SLOT(UpdateRSS()));

        mainStack->AddScreen(rssedit);
    }
    else
        delete rssedit;
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

    RSSManager *rssMan = new RSSManager();
    connect(rssMan, SIGNAL(finished()), this, SLOT(DoTreeRefresh()));
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
    gCoreContext->SaveSetting("mythnetvision.rssBackgroundFetch",
                              m_rssAutoUpdate);
}

void NetTree::ToggleTreeUpdates()
{
    m_treeAutoUpdate = !m_treeAutoUpdate;
    gCoreContext->SaveSetting("mythnetvision.backgroundFetch",
                              m_treeAutoUpdate);
}

void NetTree::customEvent(QEvent *event)
{
    if (event->type() == ThumbnailDLEvent::kEventType)
    {
        ThumbnailDLEvent *tde = (ThumbnailDLEvent *)event;

        if (!tde)
            return;

        ThumbnailData *data = tde->thumb;

        if (!data)
            return;

        QString title = data->title;
        QString file = data->url;
        uint pos = qVariantValue<uint>(data->data);

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
        NetBase::customEvent(event);
}

void NetTree::SetSubfolderData(MythGenericTree *folder)
{
    folder->SetText(QString("%1").arg(folder->visibleChildCount()),
                    "childcount");
    folder->DisplayState("subfolder", "nodetype");
}
