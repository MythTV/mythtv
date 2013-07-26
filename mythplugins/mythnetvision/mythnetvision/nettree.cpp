// qt
#include <QString>
#include <QFileInfo>
#include <QtAlgorithms>

// myth
#include <mythdate.h>
#include <mythdb.h>
#include <mythcontext.h>
#include <mythdirs.h>
#include <mythsystemlegacy.h>
#include <remoteutil.h>
#include <remotefile.h>
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

class ResultItem;
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
    : MythScreenType(parent, name),
      m_siteMap(NULL),               m_siteButtonList(NULL),
      m_siteGeneric(NULL),           m_rssGeneric(NULL),
      m_searchGeneric(NULL),         m_currentNode(NULL),
      m_noSites(NULL),               m_thumbImage(NULL),
      m_downloadable(NULL),          m_busyPopup(NULL),
      m_menuPopup(NULL),             m_popupStack(),
      m_progressDialog(NULL),        m_downloadFile(QString()),
      m_type(type)
{
    m_imageDownload = new MetadataImageDownload(this);
    m_gdt = new GrabberDownloadThread(this);
    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
    m_updateFreq = gCoreContext->GetNumSetting(
                       "mythNetTree.updateFreq", 6);
    m_rssAutoUpdate = gCoreContext->GetNumSetting(
                       "mythnetvision.rssBackgroundFetch", 0);
    m_treeAutoUpdate = gCoreContext->GetNumSetting(
                       "mythnetvision.backgroundFetch", 0);
    gCoreContext->addListener(this);
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
                SLOT(streamWebVideo(void)));
        connect(m_siteMap, SIGNAL(itemSelected(MythUIButtonListItem *)),
                SLOT(slotItemChanged(void)));
        connect(m_siteMap, SIGNAL(nodeChanged(MythGenericTree *)),
                SLOT(slotItemChanged(void)));
    }
    else
    {
        SetFocusWidget(m_siteButtonList);

        connect(m_siteButtonList, SIGNAL(itemClicked(MythUIButtonListItem *)),
                SLOT(handleSelect(MythUIButtonListItem *)));
        connect(m_siteButtonList, SIGNAL(itemSelected(MythUIButtonListItem *)),
                SLOT(slotItemChanged(void)));
    }

    return true;
}

void NetTree::Load()
{
    m_grabberList = findAllDBTreeGrabbersByHost(VIDEO_FILE);
    m_rssList = findAllDBRSS();

    fillTree();
}

void NetTree::SetCurrentNode(MythGenericTree *node)
{
    if (!node)
        return;

    m_currentNode = node;
}

void NetTree::Init()
{
    loadData();
}

NetTree::~NetTree()
{
    qDeleteAll(m_grabberList);
    m_grabberList.clear();

    if (m_siteGeneric)
    {
        delete m_siteGeneric;
        m_siteGeneric = NULL;
    }

    cleanThumbnailCacheDir();

    if (m_imageDownload)
    {
        delete m_imageDownload;
        m_imageDownload = NULL;
    }

    if (m_gdt)
    {
        delete m_gdt;
        m_gdt = NULL;
    }

    m_rssList.clear();

    qDeleteAll(m_videos);
    m_videos.clear();

    cleanCacheDir();

    gCoreContext->removeListener(this);
}

void NetTree::cleanCacheDir()
{
    QString cache = QString("%1/thumbcache")
                       .arg(GetConfDir());
    QDir cacheDir(cache);
    QStringList thumbs = cacheDir.entryList(QDir::Files);

    for (QStringList::const_iterator i = thumbs.end() - 1;
            i != thumbs.begin() - 1; --i)
    {
        QString filename = QString("%1/%2").arg(cache).arg(*i);
        LOG(VB_GENERAL, LOG_DEBUG, QString("Deleting file %1").arg(filename));
        QFileInfo fi(filename);
        QDateTime lastmod = fi.lastModified();
        if (lastmod.addDays(7) < MythDate::current())
            QFile::remove(filename);
    }
}

void NetTree::loadData(void)
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

        slotItemChanged();
    }

    if (m_siteGeneric->childCount() == 0 && m_noSites)
        m_noSites->SetVisible(true);
    else if (m_noSites)
        m_noSites->SetVisible(false);

    if (m_siteGeneric->childCount() == 0)
        runTreeEditor();
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
        item->SetText(QString("%1").arg(node->visibleChildCount()), "childcount");
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

void NetTree::handleSelect(MythUIButtonListItem *item)
{
    MythGenericTree *node = GetNodePtrFromButton(item);
    if (!node)
        return;

    int nodeInt = node->getInt();

    switch (nodeInt)
    {
        case kSubFolder:
            handleDirSelect(node);
            break;
        case kUpFolder:
            goBack();
            break;
        default:
        {
            streamWebVideo();
        }
    };
    slotItemChanged();
}

void NetTree::handleDirSelect(MythGenericTree *node)
{
    if (m_imageDownload && m_imageDownload->isRunning())
        m_imageDownload->cancel();

    SetCurrentNode(node);
    loadData();
}

bool NetTree::goBack()
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

    loadData();

    return handled;
}

bool NetTree::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Internet Video", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            showMenu();
        }
        else if (action == "ESCAPE")
        {
            if (m_type != DLG_TREE
                    && !GetMythMainWindow()->IsExitingToMain()
                    && m_currentNode != m_siteGeneric)
                handled = goBack();
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

void NetTree::createBusyDialog(QString title)
{
    if (m_busyPopup)
        return;

    QString message = title;

    m_busyPopup = new MythUIBusyDialog(message, m_popupStack,
            "nettreebusydialog");

    if (m_busyPopup->Create())
        m_popupStack->AddScreen(m_busyPopup);
    else
    {
        delete m_busyPopup;
        m_busyPopup = NULL;
    }
}

void NetTree::showMenu(void)
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
        MythGenericTree *node = GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (node)
            item = qVariantValue<ResultItem *>(node->GetData());
    }

    if (item)
    {
        if (item->GetDownloadable())
            menu->AddItem(tr("Stream Video"), SLOT(streamWebVideo()));
        menu->AddItem(tr("Open Web Link"), SLOT(showWebVideo()));

        if (item->GetDownloadable())
            menu->AddItem(tr("Save This Video"), SLOT(doDownloadAndPlay()));
    }

    menu->AddItem(tr("Scan/Manage Subscriptions"), NULL, createShowManageMenu());
    menu->AddItem(tr("Change View"), NULL, createShowViewMenu());

    MythDialogBox *menuPopup = new MythDialogBox(menu, m_popupStack, "mythnettreemenupopup");

    if (menuPopup->Create())
        m_popupStack->AddScreen(menuPopup);
    else
        delete menuPopup;
}

MythMenu* NetTree::createShowViewMenu()
{
    QString label = tr("View Options");

    MythMenu *menu = new MythMenu(label, this, "options");

    if (m_type != DLG_TREE)
        menu->AddItem(tr("Switch to List View"), SLOT(switchTreeView()));
    if (m_type != DLG_GALLERY)
        menu->AddItem(tr("Switch to Gallery View"), SLOT(switchGalleryView()));
    if (m_type != DLG_BROWSER)
        menu->AddItem(tr("Switch to Browse View"), SLOT(switchBrowseView()));

    return menu;
}

MythMenu* NetTree::createShowManageMenu()
{
    QString label = tr("Subscription Management");

    MythMenu *menu = new MythMenu(label, this, "options");


    menu->AddItem(tr("Update Site Maps"), SLOT(updateTrees()));
    menu->AddItem(tr("Update RSS"), SLOT(updateRSS()));
    menu->AddItem(tr("Manage Site Subscriptions"), SLOT(runTreeEditor()));
    menu->AddItem(tr("Manage RSS Subscriptions"), SLOT(runRSSEditor()));
    if (!m_treeAutoUpdate)
        menu->AddItem(tr("Enable Automatic Site Updates"), SLOT(toggleTreeUpdates()));
    else
        menu->AddItem(tr("Disable Automatic Site Updates"), SLOT(toggleTreeUpdates()));
//    if (!m_rssAutoUpdate)
//        menu->AddItem(tr("Enable Automatic RSS Updates"), SLOT(toggleRSSUpdates()));
//    else
//        menu->AddItem(tr("Disable Automatic RSS Updates"), SLOT(toggleRSSUpdates()));

    return menu;
}

void NetTree::switchTreeView()
{
    m_type = DLG_TREE;
    switchView();
}

void NetTree::switchGalleryView()
{
    m_type = DLG_GALLERY;
    switchView();
}

void NetTree::switchBrowseView()
{
    m_type = DLG_BROWSER;
    switchView();
}

void NetTree::switchView()
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

void NetTree::fillTree()
{
    // First let's add all the RSS

    m_rssGeneric = new MythGenericTree(
                    RSSNode, kSubFolder, false);

    // Add an upfolder
    if (m_type != DLG_TREE)
    {
          m_rssGeneric->addNode(tr("Back"), kUpFolder, true, false);
    }

    m_rssGeneric->SetData(QString("%1/mythnetvision/icons/rss.png")
         .arg(GetShareDir()));

    RSSSite::rssList::iterator i = m_rssList.begin();
    for (; i != m_rssList.end(); ++i)
    {
        ResultItem::resultList items =
                   getRSSArticles((*i)->GetTitle(), VIDEO_PODCAST);
        MythGenericTree *ret = new MythGenericTree(
                   (*i)->GetTitle(), kSubFolder, false);
        ret->SetData(qVariantFromValue(*i));
        m_rssGeneric->addNode(ret);

        // Add an upfolder
        if (m_type != DLG_TREE)
        {
            ret->addNode(tr("Back"), kUpFolder, true, false);
        }

        ResultItem::resultList::iterator it = items.begin();
        for (; it != items.end(); ++it)
        {
            AddFileNode(ret, *it);
        }
    }

    if (m_rssList.count() > 0)
        m_siteGeneric->addNode(m_rssGeneric);
    else
    {
        delete m_rssGeneric;
        m_rssGeneric = NULL;
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
        {
            ret->addNode(tr("Back"), kUpFolder, true, false);
        }

        for (QList<QPair<QString, QString> >::iterator i = paths.begin();
                i != paths.end(); ++i)
        {
            QStringList curPaths = (*i).first.split("/");
            QString dirthumb = (*i).second;
            QList<ResultItem*> videos = treePathsNodes.values(*i);
            buildGenericTree(ret, curPaths, dirthumb, videos);
        }
        m_siteGeneric->addNode(ret);
    }
}

void NetTree::buildGenericTree(MythGenericTree *dst, QStringList paths,
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
            folder = new MythGenericTree(
                     curPath, kSubFolder, false);
    }

    if (!folder)
       return;

    folder->SetData(dirthumb);
    dst->addNode(folder);

    // Add an upfolder
    if (m_type != DLG_TREE)
    {
          folder->addNode(tr("Back"), kUpFolder, true, false);
    }

    if (paths.size())
        buildGenericTree(folder, paths, dirthumb, videos);
    else
    {
        // File Handling
        for (QList<ResultItem*>::iterator it = videos.begin();
                it != videos.end(); ++it)
            AddFileNode(folder, *it);
    }
}

MythGenericTree *NetTree::AddDirNode(MythGenericTree *where_to_add,
            QString name, QString thumbnail)
{
    QString title = name;
    title.replace("&amp;", "&");
    MythGenericTree *sub_node =
            where_to_add->addNode(title, kSubFolder, false);
    sub_node->SetData(thumbnail);
    return sub_node;
}

int NetTree::AddFileNode(MythGenericTree *where_to_add, ResultItem *video)
{
    QString title = video->GetTitle();
    title.replace("&amp;", "&");
    MythGenericTree *sub_node = where_to_add->
                    addNode(title, 0, true);
    sub_node->SetData(qVariantFromValue(video));
    m_videos.append(video);
    return 1;
}

void NetTree::streamWebVideo()
{
    ResultItem *item;

    if (m_type == DLG_TREE)
        item = qVariantValue<ResultItem *>(m_siteMap->GetCurrentNode()->GetData());
    else
    {
        MythGenericTree *node = GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (!node)
            return;

        item = qVariantValue<ResultItem *>(node->GetData());
    }

    if (!item)
        return;

    if (!item->GetDownloadable())
    {
        showWebVideo();
        return;
    }

    GetMythMainWindow()->HandleMedia(
        "Internal", item->GetMediaURL(),
        item->GetDescription(), item->GetTitle(), item->GetSubtitle(),
        QString(), item->GetSeason(), item->GetEpisode(), QString(),
        item->GetTime().toInt(), item->GetDate().toString("yyyy"));
}

void NetTree::showWebVideo()
{
    ResultItem *item;

    if (m_type == DLG_TREE)
        item = qVariantValue<ResultItem *>(m_siteMap->GetCurrentNode()->GetData());
    else
    {
        MythGenericTree *node = GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (!node)
            return;

        item = qVariantValue<ResultItem *>(node->GetData());
    }

    if (!item)
        return;

    if (!item->GetPlayer().isEmpty())
    {
        QString cmd = item->GetPlayer();
        QStringList args = item->GetPlayerArguments();
        if (!args.size())
        {
            args += item->GetMediaURL();
            if (!args.size())
                args += item->GetURL();
        }
        else
        {
            args.replaceInStrings("%DIR%", QString(GetConfDir() + "/MythNetvision"));
            args.replaceInStrings("%MEDIAURL%", item->GetMediaURL());
            args.replaceInStrings("%URL%", item->GetURL());
            args.replaceInStrings("%TITLE%", item->GetTitle());
        }
        QString playerCommand = cmd + " " + args.join(" ");

        myth_system(playerCommand);
    }
    else
    {
        QString url = item->GetURL();

        LOG(VB_GENERAL, LOG_DEBUG, QString("Web URL = %1").arg(url));

        if (url.isEmpty())
            return;

        QString browser = gCoreContext->GetSetting("WebBrowserCommand", "");
        QString zoom = gCoreContext->GetSetting("WebBrowserZoomLevel", "1.0");

        if (browser.isEmpty())
        {
            ShowOkPopup(tr("No browser command set! MythNetTree needs MythBrowser "
                           "installed to display the video."));
            return;
        }

        if (browser.toLower() == "internal")
        {
            GetMythMainWindow()->HandleMedia("WebBrowser", url);
            return;
        }
        else
        {
            QString cmd = browser;
            cmd.replace("%ZOOM%", zoom);
            cmd.replace("%URL%", url);
            cmd.replace('\'', "%27");
            cmd.replace("&","\\&");
            cmd.replace(";","\\;");

            GetMythMainWindow()->AllowInput(false);
            myth_system(cmd, kMSDontDisableDrawing);
            GetMythMainWindow()->AllowInput(true);
            return;
        }
    }
}

void NetTree::doPlayVideo(QString filename)
{
    ResultItem *item;
    if (m_type == DLG_TREE)
        item = qVariantValue<ResultItem *>(m_siteMap->GetCurrentNode()->GetData());
    else
    {
        MythGenericTree *node = GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (!node)
            return;

        item = qVariantValue<ResultItem *>(node->GetData());
    }

    if (!item)
        return;

    GetMythMainWindow()->HandleMedia("Internal", filename);
}

void NetTree::slotDeleteVideo()
{
    QString message = tr("Are you sure you want to delete this file?");

    MythConfirmationDialog *confirmdialog =
            new MythConfirmationDialog(m_popupStack,message);

    if (confirmdialog->Create())
    {
        m_popupStack->AddScreen(confirmdialog);
        connect(confirmdialog, SIGNAL(haveResult(bool)),
                SLOT(doDeleteVideo(bool)));
    }
    else
        delete confirmdialog;
}

void NetTree::doDeleteVideo(bool remove)
{
    if (!remove)
        return;

    ResultItem *item;
    if (m_type == DLG_TREE)
        item = qVariantValue<ResultItem *>(m_siteMap->GetCurrentNode()->GetData());
    else
    {
        MythGenericTree *node = GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (!node)
            return;

        item = qVariantValue<ResultItem *>(node->GetData());
    }

    if (!item)
        return;

    QString filename = GetDownloadFilename(item->GetTitle(),
                                          item->GetMediaURL());

    if (filename.startsWith("myth://"))
        RemoteFile::DeleteFile(filename);
    else
    {
        QFile file(filename);
        file.remove();
    }
}

void NetTree::doDownloadAndPlay()
{
    ResultItem *item;
    if (m_type == DLG_TREE)
        item = qVariantValue<ResultItem *>(m_siteMap->GetCurrentNode()->GetData());
    else
    {
        MythGenericTree *node = GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (!node)
            return;

        item = qVariantValue<ResultItem *>(node->GetData());
    }

    if (!item)
        return;

    QString baseFilename = GetDownloadFilename(item->GetTitle(),
                                          item->GetMediaURL());

    QString finalFilename = generate_file_url("Default",
                              gCoreContext->GetMasterHostName(),
                              baseFilename);

    LOG(VB_GENERAL, LOG_INFO, QString("Downloading %1 to %2")
            .arg(item->GetMediaURL()) .arg(finalFilename));

    // Does the file already exist?
    bool exists = RemoteFile::Exists(finalFilename);

    if (exists)
    {
        doPlayVideo(finalFilename);
        return;
    }
    else
        DownloadVideo(item->GetMediaURL(), baseFilename);
}

void NetTree::DownloadVideo(QString url, QString dest)
{
    initProgressDialog();
    m_downloadFile = RemoteDownloadFile(url, "Default", dest);
}

void NetTree::initProgressDialog()
{
    QString message = tr("Downloading Video...");
    m_progressDialog = new MythUIProgressDialog(message,
               m_popupStack, "videodownloadprogressdialog");

    if (m_progressDialog->Create())
    {
        m_popupStack->AddScreen(m_progressDialog, false);
    }
    else
    {
        delete m_progressDialog;
        m_progressDialog = NULL;
    }
}

void NetTree::slotItemChanged()
{
    ResultItem *item;
    RSSSite *site;

    if (m_type == DLG_TREE)
    {
        item = qVariantValue<ResultItem *>(m_siteMap->GetCurrentNode()->GetData());
        site = qVariantValue<RSSSite *>(m_siteMap->GetCurrentNode()->GetData());
    }
    else
    {
        MythGenericTree *node = GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (!node)
            return;

        item = qVariantValue<ResultItem *>(node->GetData());
        site = qVariantValue<RSSSite *>(node->GetData());
    }

    if (item)
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
                QString sFilename = getDownloadFilename(item->GetTitle(), item->GetThumbnail());

                bool exists = QFile::exists(sFilename);
                if (exists)
                {
                    m_thumbImage->SetFilename(sFilename);
                    m_thumbImage->Load();
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
    else if (site)
    {
        ResultItem res = ResultItem(site->GetTitle(), QString(), site->GetDescription(),
              site->GetURL(), site->GetImage(), QString(), site->GetAuthor(), QDateTime(),
              0, 0, -1, QString(), QStringList(), QString(), QStringList(), 0, 0, QString(),
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
        {
            m_downloadable->Reset();
        }
    }
    else
    {
        QString title;

        if (m_type == DLG_TREE)
            title = m_siteMap->GetItemCurrent()->GetText();
        else
            title = m_siteButtonList->GetItemCurrent()->GetText();

        QString thumb;
        if (m_type == DLG_TREE)
            thumb = m_siteMap->GetCurrentNode()->
                        GetData().toString();
        else
        {
            MythGenericTree *node = GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

            if (node)
                thumb = node->GetData().toString();
        }

        ResultItem res = ResultItem(title, QString(), QString(),
              QString(), thumb, QString(), QString(), QDateTime(),
              0, 0, -1, QString(), QStringList(), QString(), QStringList(), 0, 0, QString(),
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
}

void NetTree::runTreeEditor()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    TreeEditor *treeedit = new TreeEditor(mainStack, "mythnettreeedit");

    if (treeedit->Create())
    {
        connect(treeedit, SIGNAL(itemsChanged()), this,
                       SLOT(doTreeRefresh()));

        mainStack->AddScreen(treeedit);
    }
    else
    {
        delete treeedit;
    }
}

void NetTree::runRSSEditor()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    RSSEditor *rssedit = new RSSEditor(mainStack, "mythnetrssedit");

    if (rssedit->Create())
    {
        connect(rssedit, SIGNAL(itemsChanged()), this,
                       SLOT(updateRSS()));

        mainStack->AddScreen(rssedit);
    }
    else
    {
        delete rssedit;
    }
}

void NetTree::doTreeRefresh()
{
    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = NULL;
    }

    TreeRefresh();
}

void NetTree::TreeRefresh()
{
    m_siteGeneric = new MythGenericTree("site root", 0, false);
    m_currentNode = m_siteGeneric;

    m_grabberList = findAllDBTreeGrabbers();
    m_rssList = findAllDBRSS();

    fillTree();
    loadData();
    switchView();
}

void NetTree::updateRSS()
{
    if (findAllDBRSS().isEmpty())
        return;

    QString title(tr("Updating RSS.  This could take a while..."));
    createBusyDialog(title);

    RSSManager *rssMan = new RSSManager();
    connect(rssMan, SIGNAL(finished()), this,
                   SLOT(doTreeRefresh()));
    rssMan->startTimer();
    rssMan->doUpdate();
}

void NetTree::updateTrees()
{
    if (m_grabberList.count() == 0)
        return;

    QString title(tr("Updating Site Maps.  This could take a while..."));
    createBusyDialog(title);
    m_gdt->refreshAll();
}

void NetTree::toggleRSSUpdates()
{
    m_rssAutoUpdate = !m_rssAutoUpdate;
    gCoreContext->SaveSetting("mythnetvision.rssBackgroundFetch",
                         m_rssAutoUpdate);
}

void NetTree::toggleTreeUpdates()
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
            {
                delete data;
                return;
            }

            MythUIButtonListItem *item =
                      m_siteButtonList->GetItemAt(pos);

            if (item && item->GetText() == title)
            {
                item->SetImage(file);
            }
        }

        delete data;
    }
    else if (event->type() == kGrabberUpdateEventType)
    {
        doTreeRefresh();
    }
    else if ((MythEvent::Type)(event->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)event;
        QStringList tokens = me->Message().split(" ", QString::SkipEmptyParts);

        if (tokens.isEmpty())
            return;

        if (tokens[0] == "DOWNLOAD_FILE")
        {
            QStringList args = me->ExtraDataList();
            if ((tokens.size() != 2) ||
                (args[1] != m_downloadFile))
                return;

            if (tokens[1] == "UPDATE")
            {
                QString message = tr("Downloading Video...\n"
                                     "(%1 of %2 MB)")
                                     .arg(QString::number(args[2].toInt() / 1024.0 / 1024.0, 'f', 2))
                                     .arg(QString::number(args[3].toInt() / 1024.0 / 1024.0, 'f', 2));
                m_progressDialog->SetMessage(message);
                m_progressDialog->SetTotal(args[3].toInt());
                m_progressDialog->SetProgress(args[2].toInt());
            }
            else if (tokens[1] == "FINISHED")
            {
                int fileSize  = args[2].toInt();
                int errorCode = args[4].toInt();

                if (m_progressDialog)
                    m_progressDialog->Close();

                QFileInfo file(m_downloadFile);
                if ((m_downloadFile.startsWith("myth://")))
                {
                    if ((errorCode == 0) &&
                        (fileSize > 0))
                    {
                        doPlayVideo(m_downloadFile);
                    }
                    else
                    {
                        ShowOkPopup(tr("Error downloading video to backend."));
                    }
                }
            }
        }
    }

}
