// qt
#include <QString>
#include <QProcess>
#include <QFileInfo>
#include <QtAlgorithms>

// myth
#include <mythdb.h>
#include <mythcontext.h>
#include <mythdirs.h>
#include <remoteutil.h>
#include <remotefile.h>
#include <mythprogressdialog.h>
#include <rssparse.h>
#include <netutils.h>
#include <mythrssmanager.h>
#include <netgrabbermanager.h>

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

        return 0;
    }
}

NetTree::NetTree(DialogType type, MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name),
      m_siteMap(NULL),               m_siteButtonList(NULL),
      m_noSites(NULL),               m_thumbImage(NULL),
      m_downloadable(NULL),          m_busyPopup(NULL),
      m_menuPopup(NULL),             m_popupStack(),
      m_externaldownload(NULL),      m_type(type),
      m_lock(QMutex::Recursive)
{
    m_download = new DownloadManager(this);
    m_imageDownload = new ImageDownloadManager(this);
    m_gdt = new GrabberDownloadThread(this);
    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
    m_updateFreq = gCoreContext->GetNumSetting(
                       "mythNetTree.updateFreq", 6);
    m_rssAutoUpdate = gCoreContext->GetNumSetting(
                       "mythnetvision.rssBackgroundFetch", 0);
    m_treeAutoUpdate = gCoreContext->GetNumSetting(
                       "mythnetvision.backgroundFetch", 0);
}

bool NetTree::Create()
{
    QMutexLocker locker(&m_lock);

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
        VERBOSE(VB_IMPORTANT, "Cannot load screen '" + windowName + "'");
        return false;
    }

    BuildFocusList();

    LoadInBackground();

    if (m_type == DLG_TREE)
    {
        SetFocusWidget(m_siteMap);

        connect(m_siteMap, SIGNAL(itemClicked(MythUIButtonListItem *)),
                SLOT(showWebVideo(void)));
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
    QMutexLocker locker(&m_lock);

    m_grabberList = findAllDBTreeGrabbersByHost(VIDEO);
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
    QMutexLocker locker(&m_lock);

    qDeleteAll(m_grabberList);
    m_grabberList.clear();

    if (m_siteGeneric)
    {
        delete m_siteGeneric;
        m_siteGeneric = NULL;
    }

    if (m_externaldownload)
    {
        delete m_externaldownload;
        m_externaldownload = NULL;
    }

    if (m_download)
    {
        delete m_download;
        m_download = NULL;
    }

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
}

void NetTree::cleanCacheDir()
{
    QMutexLocker locker(&m_lock);

    QString cache = QString("%1/MythNetvision/thumbcache")
                       .arg(GetConfDir());
    QDir cacheDir(cache);
    QStringList thumbs = cacheDir.entryList(QDir::Files);

    for (QStringList::const_iterator i = thumbs.end() - 1;
            i != thumbs.begin() - 1; --i)
    {
        QString filename = QString("%1/%2").arg(cache).arg(*i);
        VERBOSE(VB_GENERAL|VB_EXTRA, QString("Deleting file %1").arg(filename));
        QFileInfo fi(filename);
        QDateTime lastmod = fi.lastModified();
        if (lastmod.addDays(7) < QDateTime::currentDateTime())
            QFile::remove(filename);
    }
}

void NetTree::loadData(void)
{
    QMutexLocker locker(&m_lock);

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
        m_imageDownload->start();

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
    QMutexLocker locker(&m_lock);

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
        item->SetText(node->getString(), "title");
        item->SetText(node->getString());
    }
    else if (nodeInt == kUpFolder)
    {
        item->DisplayState("upfolder", "nodetype");
        item->SetText(node->getString(), "title");
        item->SetText(node->getString());
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

        MetadataMap metadataMap;
        video->toMap(metadataMap);
        item->SetTextFromMap(metadataMap);

        int pos;
        if (m_type == DLG_TREE)
            pos = 0;
        else
            pos = m_siteButtonList->GetItemPos(item);

        QString dlfile = GetThumbnailFilename(video->GetThumbnail(), video->GetTitle());

        if (QFile::exists(dlfile))
            item->SetImage(dlfile);
        else
            m_imageDownload->addURL(video->GetTitle(), video->GetThumbnail(), pos);
    }
    else
    {
        item->SetText(node->getString());
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
                                                      node->getString());
                if (QFile::exists(dlfile))
                    item->SetImage(dlfile);
                else
                    m_imageDownload->addURL(node->getString(), tpath, pos);
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
    QMutexLocker locker(&m_lock);

    MythGenericTree *node = GetNodePtrFromButton(item);
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
            showWebVideo();
        }
    };
    slotItemChanged();
}

void NetTree::handleDirSelect(MythGenericTree *node)
{
    QMutexLocker locker(&m_lock);

    if (m_imageDownload && m_imageDownload->isRunning())
        m_imageDownload->cancel();

    SetCurrentNode(node);
    loadData();
}

bool NetTree::goBack()
{
    QMutexLocker locker(&m_lock);

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
    QMutexLocker locker(&m_lock);

    QString label = tr("Playback/Download Options");

    MythDialogBox *menuPopup = new MythDialogBox(label, m_popupStack,
                                                    "mythnettreemenupopup");

    ResultItem *item = NULL;
    if (m_type == DLG_TREE)
        item = qVariantValue<ResultItem *>(m_siteMap->GetCurrentNode()->GetData());
    else
    {
        MythGenericTree *node = GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (node)
            item = qVariantValue<ResultItem *>(node->GetData());
    }

    if (menuPopup->Create())
    {
        m_popupStack->AddScreen(menuPopup);

        if (item)
        {
            menuPopup->AddButton(tr("Open Web Link"), SLOT(showWebVideo()));

            if (item->GetDownloadable())
                menuPopup->AddButton(tr("Save This Video"), SLOT(doDownloadAndPlay()));
        }

        menuPopup->AddButton(tr("Scan/Manage Subscriptions"), SLOT(showManageMenu()), true);
        menuPopup->AddButton(tr("Change View"), SLOT(showViewMenu()), true);

        menuPopup->SetReturnEvent(this, "options");
    }
    else
    {
        delete menuPopup;
    }
}

void NetTree::showViewMenu()
{
    QMutexLocker locker(&m_lock);

    QString label = tr("View Options");

    MythDialogBox *menuPopup = new MythDialogBox(label, m_popupStack,
                                                    "mythnetvisionmenupopup");

    if (menuPopup->Create())
    {
        m_popupStack->AddScreen(menuPopup);

        menuPopup->SetReturnEvent(this, "options");

        if (m_type != DLG_TREE)
            menuPopup->AddButton(tr("Switch to List View"), SLOT(switchTreeView()));
        if (m_type != DLG_GALLERY)
            menuPopup->AddButton(tr("Switch to Gallery View"), SLOT(switchGalleryView()));
        if (m_type != DLG_BROWSER)
            menuPopup->AddButton(tr("Switch to Browse View"), SLOT(switchBrowseView()));
    }
    else
    {
        delete menuPopup;
    }
}

void NetTree::showManageMenu()
{
    QMutexLocker locker(&m_lock);

    QString label = tr("Subscription Management");

    MythDialogBox *menuPopup = new MythDialogBox(label, m_popupStack,
                                                    "mythnetvisionmanagepopup");

    if (menuPopup->Create())
    {
        m_popupStack->AddScreen(menuPopup);

        menuPopup->SetReturnEvent(this, "options");

        menuPopup->AddButton(tr("Update Site Maps"), SLOT(updateTrees()));
        menuPopup->AddButton(tr("Update RSS"), SLOT(updateRSS()));
        menuPopup->AddButton(tr("Manage Site Subscriptions"), SLOT(runTreeEditor()));
        menuPopup->AddButton(tr("Manage RSS Subscriptions"), SLOT(runRSSEditor()));
        if (!m_treeAutoUpdate)
            menuPopup->AddButton(tr("Enable Automatic Site Updates"), SLOT(toggleTreeUpdates()));
        else 
            menuPopup->AddButton(tr("Disable Automatic Site Updates"), SLOT(toggleTreeUpdates()));
//        if (!m_rssAutoUpdate) 
//            menuPopup->AddButton(tr("Enable Automatic RSS Updates"), SLOT(toggleRSSUpdates()));
//        else
//            menuPopup->AddButton(tr("Disable Automatic RSS Updates"), SLOT(toggleRSSUpdates()));
    }
    else
    {
        delete menuPopup;
    }
}

void NetTree::switchTreeView()
{
    QMutexLocker locker(&m_lock);

    m_type = DLG_TREE;
    switchView();
}

void NetTree::switchGalleryView()
{
    QMutexLocker locker(&m_lock);

    m_type = DLG_GALLERY;
    switchView();
}

void NetTree::switchBrowseView()
{
    QMutexLocker locker(&m_lock);

    m_type = DLG_BROWSER;
    switchView();
}

void NetTree::switchView()
{
    QMutexLocker locker(&m_lock);

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
    QMutexLocker locker(&m_lock);

    // First let's add all the RSS

    m_rssGeneric = new MythGenericTree(
                    RSSNode, kSubFolder, false);

    // Add an upfolder
    if (m_type != DLG_TREE)
    {
          m_rssGeneric->addNode(QString(tr("Back")), kUpFolder, true, false);
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
            ret->addNode(QString(tr("Back")), kUpFolder, true, false);
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
                           getTreeArticles((*i)->GetTitle(), VIDEO);

        QList< QPair<QString,QString> > paths = treePathsNodes.uniqueKeys();

        MythGenericTree *ret = new MythGenericTree(
                   (*i)->GetTitle(), kSubFolder, false);
        QString thumb = QString("%1mythnetvision/icons/%2").arg(GetShareDir())
                            .arg((*i)->GetImage());
        ret->SetData(qVariantFromValue(thumb));

        // Add an upfolder
        if (m_type != DLG_TREE)
        {
            ret->addNode(QString(tr("Back")), kUpFolder, true, false);
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
          folder->addNode(QString(tr("Back")), kUpFolder, true, false);
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

void NetTree::showWebVideo()
{
    QMutexLocker locker(&m_lock);

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

        VERBOSE(VB_GENERAL|VB_EXTRA, QString("Web URL = %1").arg(url));

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
            myth_system(cmd, MYTH_SYSTEM_DONT_BLOCK_PARENT);
            GetMythMainWindow()->AllowInput(true);
            return;
        }
    }
}

void NetTree::doPlayVideo()
{
    QMutexLocker locker(&m_lock);

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

    GetMythMainWindow()->HandleMedia("Internal", getDownloadFilename(item));
}

void NetTree::slotDeleteVideo()
{
    QMutexLocker locker(&m_lock);

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
    QMutexLocker locker(&m_lock);

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

    QString filename = getDownloadFilename(item);

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
    QMutexLocker locker(&m_lock);

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

    if (m_download->isRunning())
    {
        QString message = tr("Download already running.  Try again "
                             "when the download is finished.");

        MythConfirmationDialog *okPopup =
            new MythConfirmationDialog(m_popupStack, message, false);

        if (okPopup->Create())
            m_popupStack->AddScreen(okPopup);
        else
            delete okPopup;

        return;
    }

    QString filename = getDownloadFilename(item);

    VERBOSE(VB_GENERAL, QString("Downloading %1").arg(filename));

    // Does the file already exist?
    bool exists;
    if (filename.startsWith("myth://"))
        exists = RemoteFile::Exists(filename);
    else
        exists = QFile::exists(filename);

    if (exists)
    {
        doPlayVideo();
        return;
    }

    // Initialize the download
    m_download->addDL(item);
    if (!m_download->isRunning())
        m_download->start();
}

QString NetTree::getDownloadFilename(ResultItem *item)
{
    QByteArray urlarr(item->GetMediaURL().toLatin1());
    quint16 urlChecksum = qChecksum(urlarr.data(), urlarr.length());
    QByteArray titlearr(item->GetTitle().toLatin1());
    quint16 titleChecksum = qChecksum(titlearr.data(), titlearr.length());
    QUrl qurl(item->GetMediaURL());
    QString ext = QFileInfo(qurl.path()).suffix();
    QString basefilename = QString("download_%1_%2.%3")
                           .arg(QString::number(urlChecksum))
                           .arg(QString::number(titleChecksum)).arg(ext);


    QString finalFilename = GetConfDir() + "/" + basefilename;

    return finalFilename;
}

void NetTree::slotItemChanged()
{
    QMutexLocker locker(&m_lock);

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
        MetadataMap metadataMap;
        item->toMap(metadataMap);
        SetTextFromMap(metadataMap);

        if (!item->GetThumbnail().isEmpty() && m_thumbImage)
        {
            // Put some thumbnail handling stuff if I ever figure it out.

            QString fileprefix = GetConfDir();

            QDir dir(fileprefix);
            if (!dir.exists())
                dir.mkdir(fileprefix);

            fileprefix += "/MythNetvision";

            dir = QDir(fileprefix);
            if (!dir.exists())
                dir.mkdir(fileprefix);

            fileprefix += "/thumbcache";

            dir = QDir(fileprefix);
            if (!dir.exists())
               dir.mkdir(fileprefix);

            QString url = item->GetThumbnail();
            QString title = item->GetTitle();
            QString sFilename = QString("%1/%2_%3")
                .arg(fileprefix)
                .arg(qChecksum(url.toLocal8Bit().constData(),
                               url.toLocal8Bit().size()))
                .arg(qChecksum(title.toLocal8Bit().constData(),
                               title.toLocal8Bit().size()));

            // Hello, I am code that causes hangs.
            bool exists = QFile::exists(sFilename);
            if (exists)
            {
                m_thumbImage->SetFilename(sFilename);
                m_thumbImage->Load();
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
        ResultItem *res = new ResultItem(site->GetTitle(), QString(), site->GetDescription(),
              site->GetURL(), site->GetImage(), QString(), site->GetAuthor(), QDateTime(),
              0, 0, -1, QString(), QStringList(), QString(), QStringList(), 0, 0, QString(),
              0, QStringList(), 0, 0, 0);

        MetadataMap metadataMap;
        res->toMap(metadataMap);
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

        ResultItem *res = new ResultItem(title, QString(), QString(),
              QString(), QString(), QString(), QString(), QDateTime(),
              0, 0, -1, QString(), QStringList(), QString(), QStringList(), 0, 0, QString(),
              0, QStringList(), 0, 0, 0);

        MetadataMap metadataMap;
        res->toMap(metadataMap);
        SetTextFromMap(metadataMap);

        if (m_thumbImage)
        {
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

            if (!thumb.startsWith("http://"))
            {
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
                QString fileprefix = GetConfDir();

                QDir dir(fileprefix);
                if (!dir.exists())
                    dir.mkdir(fileprefix);

                fileprefix += "/MythNetvision";

                dir = QDir(fileprefix);
                if (!dir.exists())
                    dir.mkdir(fileprefix);

                fileprefix += "/thumbcache";

                dir = QDir(fileprefix);
                if (!dir.exists())
                   dir.mkdir(fileprefix);

                QString url = thumb;
                QString title;
                if (m_type == DLG_TREE)
                    title = m_siteMap->GetItemCurrent()->GetText();
                else
                    title = m_siteButtonList->GetItemCurrent()->GetText();
                QString sFilename = QString("%1/%2_%3")
                    .arg(fileprefix)
                    .arg(qChecksum(url.toLocal8Bit().constData(),
                                   url.toLocal8Bit().size()))
                    .arg(qChecksum(title.toLocal8Bit().constData(),
                                   title.toLocal8Bit().size()));

                // Hello, I am code that causes hangs.
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
    m_gdt->start();
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
    QMutexLocker locker(&m_lock);

    if (event->type() == ImageDLEvent::kEventType)
    {
        ImageDLEvent *ide = (ImageDLEvent *)event;

        ImageData *id = ide->imageData;
        if (!id)
            return;

        if (m_type == DLG_TREE)
        {
            if (id->title == m_siteMap->GetCurrentNode()->getString() &&
                m_thumbImage)
            {
                m_thumbImage->SetFilename(id->filename);
                m_thumbImage->Load();
                m_thumbImage->Show();
            }
        }
        else
        {
            if (!((uint)m_siteButtonList->GetCount() >= id->pos))
                return;

            MythUIButtonListItem *item =
                      m_siteButtonList->GetItemAt(id->pos);

            if (item && item->GetText() == id->title)
            {
                item->SetImage(id->filename);
            }
        }

        delete id;
    }
    else if (event->type() == VideoDLEvent::kEventType)
    {
        VideoDLEvent *vde = (VideoDLEvent *)event;

        VideoDL *dl = vde->videoDL;
        if (!dl)
            return;

        GetMythMainWindow()->HandleMedia("Internal", dl->filename);
        delete dl;
    }
    else if (event->type() == kGrabberUpdateEventType)
    {
        doTreeRefresh();
    }
}
