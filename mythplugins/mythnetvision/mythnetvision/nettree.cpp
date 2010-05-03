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

// mythnetvision
#include "treedbutil.h"
#include "treeeditor.h"
#include "parse.h"
#include "nettree.h"
#include "netutils.h"
#include "rssdbutil.h"
#include "rsseditor.h"
#include "rssmanager.h"
#include "grabbermanager.h"

class ResultVideo;
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
      m_title(NULL),                 m_description(NULL),
      m_url(NULL),                   m_thumbnail(NULL),
      m_mediaurl(NULL),              m_author(NULL),
      m_date(NULL),                  m_time(NULL),
      m_filesize(NULL),              m_filesize_str(NULL),
      m_rating(NULL),                m_noSites(NULL),
      m_width(NULL),                 m_height(NULL),
      m_resolution(NULL),            m_countries(NULL),
      m_season(NULL),                m_episode(NULL),
      m_s00e00(NULL),                m_00x00(NULL),
      m_thumbImage(NULL),            m_downloadable(NULL),
      m_busyPopup(NULL),             m_menuPopup(NULL),
      m_popupStack(),                m_externaldownload(NULL),
      m_type(type),                  m_lock(QMutex::Recursive)
{
    m_download = new DownloadManager(this);
    m_imageDownload = new ImageDownloadManager(this);
    m_gdt = new GrabberDownloadThread(this);
    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
    m_updateFreq = gContext->GetNumSetting(
                       "mythNetTree.updateFreq", 6);
    m_rssAutoUpdate = gContext->GetNumSetting(
                       "mythnetvision.rssBackgroundFetch", 0);
    m_treeAutoUpdate = gContext->GetNumSetting(
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

    UIUtilW::Assign(this, m_title, "title");
    UIUtilW::Assign(this, m_description, "description");
    UIUtilW::Assign(this, m_url, "url");
    UIUtilW::Assign(this, m_thumbnail, "thumbnail");
    UIUtilW::Assign(this, m_mediaurl, "mediaurl");
    UIUtilW::Assign(this, m_author, "author");
    UIUtilW::Assign(this, m_date, "date");
    UIUtilW::Assign(this, m_time, "time");
    UIUtilW::Assign(this, m_filesize, "filesize");
    UIUtilW::Assign(this, m_filesize_str, "filesize_str");
    UIUtilW::Assign(this, m_rating, "rating");
    UIUtilW::Assign(this, m_noSites, "nosites");
    UIUtilW::Assign(this, m_width, "width");
    UIUtilW::Assign(this, m_height, "height");
    UIUtilW::Assign(this, m_resolution, "resolution");
    UIUtilW::Assign(this, m_countries, "countries");
    UIUtilW::Assign(this, m_season, "season");
    UIUtilW::Assign(this, m_episode, "episode");
    UIUtilW::Assign(this, m_s00e00, "s##e##");
    UIUtilW::Assign(this, m_00x00, "##x##");

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

    m_grabberList = findAllDBTreeGrabbers();
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
    ResultVideo *video = qVariantValue<ResultVideo *>(node->GetData());

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
        item->SetText(video->GetTitle(), "title");
        item->SetText(video->GetAuthor(), "author");
        item->SetText(video->GetDate().toString(gContext->
                    GetSetting("DateFormat", "yyyy-MM-dd hh:mm")), "date");
        item->SetText(video->GetDescription(), "description");
        item->SetText(video->GetURL(), "url");
        item->SetText(QString::number(video->GetWidth()), "width");
        item->SetText(QString::number(video->GetHeight()), "height");
        item->SetText(QString("%1x%2").arg(video->GetWidth())
                      .arg(video->GetHeight()), "resolution");
        item->SetText(video->GetCountries().join(","), "countries");
        item->SetText(QString::number(video->GetSeason()), "season");
        item->SetText(QString::number(video->GetEpisode()), "episode");

        item->SetText(QString("s%1e%2").arg(GetDisplaySeasonEpisode
                             (video->GetSeason(), 2)).arg(
                             GetDisplaySeasonEpisode(video->GetEpisode(), 2)),
                             "s##e##");
        item->SetText(QString("%1x%2").arg(GetDisplaySeasonEpisode
                             (video->GetSeason(), 1)).arg(
                             GetDisplaySeasonEpisode(video->GetEpisode(), 2)),
                             "##x##");
        off_t bytes = video->GetFilesize();
        item->SetText(QString::number(bytes), "filesize");
        QString tmpSize;

        tmpSize.sprintf("%0.2f ", bytes / 1024.0 / 1024.0);
        tmpSize += QObject::tr("MB", "Megabytes");
        if (bytes == 0 && !video->GetDownloadable())
            item->SetText(tr("Web Only"), "filesize_str");
        else if (bytes == 0 && video->GetDownloadable())
            item->SetText(tr("Downloadable"), "filesize_str");
        else
            item->SetText(tmpSize, "filesize_str");

        int pos;
        if (m_type == DLG_TREE)
            pos = 0;
        else
            pos = m_siteButtonList->GetItemPos(item);

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

                m_imageDownload->addURL(node->getString(), tpath, pos);
            }
            else if (tpath != "0")
                item->SetImage(node->GetData().toString());
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

    ResultVideo *item = NULL;
    if (m_type == DLG_TREE)
        item = qVariantValue<ResultVideo *>(m_siteMap->GetCurrentNode()->GetData());
    else
    {
        MythGenericTree *node = GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (node)
            item = qVariantValue<ResultVideo *>(node->GetData());
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
        gContext->SaveSetting("mythnetvision.ViewMode", m_type);
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
        ResultVideo::resultList items =
                   getRSSArticles((*i)->GetTitle());
        MythGenericTree *ret = new MythGenericTree(
                   (*i)->GetTitle(), kSubFolder, false);
        ret->SetData(qVariantFromValue(*i));
        m_rssGeneric->addNode(ret);

        // Add an upfolder
        if (m_type != DLG_TREE)
        {
            ret->addNode(QString(tr("Back")), kUpFolder, true, false);
        }

        ResultVideo::resultList::iterator it = items.begin();
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

        QMultiMap<QPair<QString,QString>, ResultVideo*> treePathsNodes =
                           getTreeArticles((*i)->GetTitle());

        QList< QPair<QString,QString> > paths = treePathsNodes.uniqueKeys();

        MythGenericTree *ret = new MythGenericTree(
                   (*i)->GetTitle(), kSubFolder, false);
        ret->SetData(qVariantFromValue((*i)->GetImage()));

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
            QList<ResultVideo*> videos = treePathsNodes.values(*i);
            buildGenericTree(ret, curPaths, dirthumb, videos);
        }
        m_siteGeneric->addNode(ret);
    }
}

void NetTree::buildGenericTree(MythGenericTree *dst, QStringList paths,
                               QString dirthumb, QList<ResultVideo*> videos)
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
        for (QList<ResultVideo*>::iterator it = videos.begin();
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

int NetTree::AddFileNode(MythGenericTree *where_to_add, ResultVideo *video)
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

    ResultVideo *item;

    if (m_type == DLG_TREE)
        item = qVariantValue<ResultVideo *>(m_siteMap->GetCurrentNode()->GetData());
    else
    {
        MythGenericTree *node = GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (!node)
            return;

        item = qVariantValue<ResultVideo *>(node->GetData());
    }

    if (!item)
        return;

    QString url = item->GetURL();

    VERBOSE(VB_GENERAL|VB_EXTRA, QString("Web URL = %1").arg(url));

    if (url.isEmpty())
        return;

    QString browser = gContext->GetSetting("WebBrowserCommand", "");
    QString zoom = gContext->GetSetting("WebBrowserZoomLevel", "1.0");

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
        gContext->GetMainWindow()->AllowInput(true);
        return;
    }
}

void NetTree::doPlayVideo()
{
    QMutexLocker locker(&m_lock);

    ResultVideo *item;
    if (m_type == DLG_TREE)
        item = qVariantValue<ResultVideo *>(m_siteMap->GetCurrentNode()->GetData());
    else
    {
        MythGenericTree *node = GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (!node)
            return;

        item = qVariantValue<ResultVideo *>(node->GetData());
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

    ResultVideo *item;
    if (m_type == DLG_TREE)
        item = qVariantValue<ResultVideo *>(m_siteMap->GetCurrentNode()->GetData());
    else
    {
        MythGenericTree *node = GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (!node)
            return;

        item = qVariantValue<ResultVideo *>(node->GetData());
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

    ResultVideo *item;
    if (m_type == DLG_TREE)
        item = qVariantValue<ResultVideo *>(m_siteMap->GetCurrentNode()->GetData());
    else
    {
        MythGenericTree *node = GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (!node)
            return;

        item = qVariantValue<ResultVideo *>(node->GetData());
    }

    if (!item)
        return;

    if (!item->GetPlayer().isEmpty())
    {
        m_externaldownload = new QProcess();
        QString cmd = item->GetPlayer();
        QStringList args = item->GetPlayerArguments();
        args.replaceInStrings("%DIR%", QString(GetConfDir() + "/MythNetvision"));
        args.replaceInStrings("%MEDIAURL%", item->GetMediaURL());
        args.replaceInStrings("%URL%", item->GetURL());
        args.replaceInStrings("%TITLE%", item->GetTitle());

        m_externaldownload->setReadChannel(QProcess::StandardOutput);

//        connect(externalplay, SIGNAL(finished(int, QProcess::ExitStatus)),
//                  SLOT(slotProcessSearchExit(int, QProcess::ExitStatus)));

        m_externaldownload->start(cmd, args);
//        QByteArray result = externalplay.readAll();
//        QString resultString(result);
    }
    else
    {
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
}

QString NetTree::getDownloadFilename(ResultVideo *item)
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

    ResultVideo *item;
    RSSSite *site;

    if (m_type == DLG_TREE)
    {
        item = qVariantValue<ResultVideo *>(m_siteMap->GetCurrentNode()->GetData());
        site = qVariantValue<RSSSite *>(m_siteMap->GetCurrentNode()->GetData());
    }
    else
    {
        MythGenericTree *node = GetNodePtrFromButton(m_siteButtonList->GetItemCurrent());

        if (!node)
            return;

        item = qVariantValue<ResultVideo *>(node->GetData());
        site = qVariantValue<RSSSite *>(node->GetData());
    }

    if (item)
    {
        if (m_title)
            m_title->SetText(item->GetTitle());
        if (m_description)
            m_description->SetText(item->GetDescription());
        if (m_url)
            m_url->SetText(item->GetURL());
        if (m_thumbnail)
            m_thumbnail->SetText(item->GetThumbnail());
        if (m_mediaurl)
            m_mediaurl->SetText(item->GetMediaURL());
        if (m_author)
            m_author->SetText(item->GetAuthor());
        if (m_date)
            m_date->SetText(item->GetDate().toString(gContext->
                    GetSetting("DateFormat", "yyyy-MM-dd hh:mm")));
        if (m_time)
            m_time->SetText(item->GetTime());
        if (m_rating)
            m_rating->SetText(item->GetRating());
        if (m_width)
            m_width->SetText(QString::number(item->GetWidth()));
        if (m_height)
            m_height->SetText(QString::number(item->GetHeight()));
        if (m_resolution)
            m_resolution->SetText(QString("%1x%2")
                          .arg(item->GetWidth()).arg(item->GetHeight()));
        if (m_countries)
            m_countries->SetText(item->GetCountries().join(","));
        if (m_season)
            m_season->SetText(QString::number(item->GetSeason()));
        if (m_episode)
            m_episode->SetText(QString::number(item->GetEpisode()));
        if (m_s00e00)
            m_s00e00->SetText(QString("s%1e%2").arg(GetDisplaySeasonEpisode
                             (item->GetSeason(), 2)).arg(
                             GetDisplaySeasonEpisode(item->GetEpisode(), 2)));
        if (m_00x00)
            m_00x00->SetText(QString("%1x%2").arg(GetDisplaySeasonEpisode
                             (item->GetSeason(), 1)).arg(
                             GetDisplaySeasonEpisode(item->GetEpisode(), 2)));


        if (m_filesize || m_filesize_str)
        {
            off_t bytes = item->GetFilesize();
            if (m_filesize)
            {
                if (bytes == 0 && !item->GetDownloadable())
                    m_filesize_str->SetText(tr("Web Only"));
                else if (bytes == 0 && item->GetDownloadable())
                    m_filesize_str->SetText(tr("Downloadable"));
                else
                    m_filesize->SetText(QString::number(bytes));
            }
            if (m_filesize_str)
            {
                QString tmpSize;

                tmpSize.sprintf("%0.2f ", bytes / 1024.0 / 1024.0);
                tmpSize += QObject::tr("MB", "Megabytes");
                if (bytes == 0 && !item->GetDownloadable())
                    m_filesize_str->SetText(tr("Web Only"));
                else if (bytes == 0 && item->GetDownloadable())
                    m_filesize_str->SetText(tr("Downloadable"));
                else
                    m_filesize_str->SetText(tmpSize);
            }
        }
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
        if (m_title)
            m_title->SetText(site->GetTitle());
        if (m_description)
            m_description->SetText(site->GetDescription());
        if (m_url)
            m_url->SetText(site->GetURL());
        if (m_thumbnail)
            m_thumbnail->SetText(site->GetImage());
        if (m_author)
            m_author->SetText(site->GetAuthor());
        if (!site->GetImage().isEmpty() && m_thumbImage)
        {
            m_thumbImage->SetFilename(site->GetImage());
            m_thumbImage->Load();
        }
        else if (m_thumbImage)
            m_thumbImage->Reset();
        if (m_mediaurl)
            m_mediaurl->SetText("");
        if (m_date)
            m_date->SetText("");
        if (m_time)
            m_time->SetText("");
        if (m_rating)
            m_rating->SetText("");
        if (m_countries)
            m_countries->SetText("");
        if (m_season)
            m_season->SetText("");
        if (m_episode)
            m_episode->SetText("");
        if (m_s00e00)
            m_s00e00->SetText("");
        if (m_00x00)
            m_00x00->SetText("");
        if (m_filesize)
            m_filesize->SetText("");
        if (m_filesize_str)
            m_filesize_str->SetText("");
    }
    else
    {
        if (m_title)
        {
            if (m_type == DLG_TREE)
                m_title->SetText(m_siteMap->GetItemCurrent()->GetText());
            else
                m_title->SetText(m_siteButtonList->GetItemCurrent()->GetText());
        }
        if (m_description)
            m_description->SetText("");
        if (m_url)
            m_url->SetText("");
        if (m_thumbnail)
            m_thumbnail->SetText("");
        if (m_author)
            m_author->SetText("");
        if (m_mediaurl)
            m_mediaurl->SetText("");
        if (m_date)
            m_date->SetText("");
        if (m_time)
            m_time->SetText("");
        if (m_rating)
            m_rating->SetText("");
        if (m_filesize)
            m_filesize->SetText("");
        if (m_countries)
            m_countries->SetText("");
        if (m_season)
            m_season->SetText("");
        if (m_episode)
            m_episode->SetText("");
        if (m_s00e00)
            m_s00e00->SetText("");
        if (m_00x00)
            m_00x00->SetText("");
        if (m_filesize_str)
            m_filesize_str->SetText("");
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
    gContext->SaveSetting("mythnetvision.rssBackgroundFetch",
                         m_rssAutoUpdate);
}

void NetTree::toggleTreeUpdates()
{
    m_treeAutoUpdate = !m_treeAutoUpdate;
    gContext->SaveSetting("mythnetvision.backgroundFetch",
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
