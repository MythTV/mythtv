
// C++ headers
#include <unistd.h>
#include <cstdlib>
#include <cmath>

// QT headers
#include <QApplication>
#include <QStringList>
#include <QList>
#include <QFile>
#include <QDir>
#include <QPainter>

// MythTV headers
#include <mythtv/mythcontext.h>
#include <mythtv/compat.h>
#include <mythtv/mythdirs.h>

// Mythui headers
#include <mythtv/libmythui/mythuihelper.h>

// Mythvideo headers
#include "videodlg.h"
#include "videotree.h"
#include "globals.h"
#include "videofilter.h"
#include "metadata.h"
#include "videolist.h"
#include "videoutils.h"
#include "imagecache.h"
#include "parentalcontrols.h"
#include "editmetadata.h"
#include "metadatalistmanager.h"
#include "videopopups.h"

VideoDialog::VideoDialog(MythScreenStack *parent, const QString &name,
                         VideoList *video_list, DialogType type)
            : MythScreenType(parent, name)
{
    m_menuPopup = NULL;
    m_busyPopup = NULL;
    m_popupStack = GetMythMainWindow()->GetStack("popup stack");

    m_type = type;
    m_videoList = video_list;
    m_treeLoaded = false;

    m_currentParentalLevel = new ParentalLevel();

    VideoFilterSettings video_filter(true, name);
    m_videoList->setCurrentVideoFilter(video_filter);
    m_rootNode = m_currentNode = NULL;

    m_isFileBrowser = gContext->GetNumSetting("VideoDialogNoDB", 0);
    m_isFlatList = false;

    m_videoButtonList = NULL;
    m_titleText = m_novideoText = m_positionText = NULL;
    m_parentalLevelState = m_videoLevelState = NULL;
    m_crumbText = NULL;
    m_coverImage = NULL;

    m_artDir = gContext->GetSetting("VideoArtworkDir");

    if (gContext->
        GetNumSetting("mythvideo.ParentalLevelFromRating", 0))
    {
        for (ParentalLevel sl(ParentalLevel::plLowest);
            sl.GetLevel() <= ParentalLevel::plHigh && sl.good(); ++sl)
        {
            QString ratingstring = gContext->GetSetting(
                            QString("mythvideo.AutoR2PL%1").arg(sl.GetLevel()));
            QStringList ratings = ratingstring.split(':', QString::SkipEmptyParts);

            for (QStringList::const_iterator p = ratings.begin();
                p != ratings.end(); ++p)
            {
                m_rating_to_pl.push_back(
                    parental_level_map::value_type(*p, sl.GetLevel()));
            }
        }
        m_rating_to_pl.sort(std::not2(rating_to_pl_less()));
    }

    connect(&m_url_dl_timer,
            SIGNAL(SigTimeout(const QString &, Metadata *)),
            SLOT(OnPosterDownloadTimeout(const QString &, Metadata *)));
    connect(&m_url_operator,
            SIGNAL(SigFinished(Q3NetworkOperation *, Metadata *)),
            SLOT(OnPosterCopyFinished(Q3NetworkOperation *,
                                    Metadata *)));

    m_scanner = NULL;
}

VideoDialog::~VideoDialog()
{
    if (m_currentParentalLevel)
        delete m_currentParentalLevel;

    if (m_scanner)
        delete m_scanner;
}

bool VideoDialog::Create(void)
{
    bool foundtheme = false;

    QString windowName = "videogallery";

    int flatlistDefault = 0;

    switch (m_type)
    {
        case DLG_DEFAULT:
            m_type = static_cast<DialogType>(
                    gContext->GetNumSetting("Default MythVideo View",
                                            DLG_GALLERY));
        case DLG_BROWSER:
            windowName = "browser";
            flatlistDefault = 1;
            break;
        case DLG_GALLERY:
            windowName = "gallery";
            break;
        case DLG_TREE:
            windowName = "tree";
            break;
        case DLG_MANAGER:
            m_isFlatList = gContext->GetNumSetting("mythvideo.db_folder_view", 1);
            windowName = "manager";
            flatlistDefault = 1;
            break;
        default:
            break;
    }

    m_isFlatList = gContext->GetNumSetting(
                    QString("mythvideo.folder_view_%1").arg(m_type),
                    flatlistDefault);

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("video-ui.xml", windowName, this);

    if (!foundtheme)
        return false;

    m_videoButtonList = dynamic_cast<MythUIButtonList *> (GetChild("videos"));

    m_titleText = dynamic_cast<MythUIText *> (GetChild("title"));
    m_novideoText = dynamic_cast<MythUIText *> (GetChild("novideos"));
    m_positionText = dynamic_cast<MythUIText *> (GetChild("position"));
    m_crumbText = dynamic_cast<MythUIText *> (GetChild("breadcrumbs"));

    m_coverImage = dynamic_cast<MythUIImage *> (GetChild("coverimage"));

    m_parentalLevelState = dynamic_cast<MythUIStateType *>
                                                    (GetChild("parentallevel"));
    m_videoLevelState = dynamic_cast<MythUIStateType *>
                                                    (GetChild("videolevel"));

    if (m_novideoText)
        m_novideoText->SetText(tr("No Videos Available"));

    if (!m_videoButtonList)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical elements.");
        return false;
    }

    if (m_parentalLevelState)
        m_parentalLevelState->DisplayState("None");

    connect(m_videoButtonList, SIGNAL(itemClicked( MythUIButtonListItem*)),
            this, SLOT( handleSelect(MythUIButtonListItem*)));
    connect(m_videoButtonList, SIGNAL(itemSelected( MythUIButtonListItem*)),
            this, SLOT( UpdateText(MythUIButtonListItem*)));
    connect(m_currentParentalLevel, SIGNAL(LevelChanged()),
            this, SLOT(reloadData()));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist.");

    SetFocusWidget(m_videoButtonList);
    m_videoButtonList->SetActive(true);

    int level = gContext->GetNumSetting("VideoDefaultParentalLevel", 1);
    m_currentParentalLevel->SetLevel((ParentalLevel::Level)level);

    return true;
}

void VideoDialog::refreshData()
{
    fetchVideos();
    loadData();

    if (m_parentalLevelState)
    {
        QString statename = "";
        switch (m_currentParentalLevel->GetLevel())
        {
            case ParentalLevel::plLowest :
                statename = "Lowest";
                break;
            case ParentalLevel::plLow :
                statename = "Low";
                break;
            case ParentalLevel::plMedium :
                statename = "Medium";
                break;
            case ParentalLevel::plHigh :
                statename = "High";
                break;
            default :
                statename = "None";
        }
        m_parentalLevelState->DisplayState(statename);
    }

    if (m_novideoText)
        m_novideoText->SetVisible(!m_treeLoaded);
}

void VideoDialog::reloadData()
{
    m_treeLoaded = false;
    refreshData();
}

void VideoDialog::loadData()
{
    if (!m_treeLoaded)
        return;

    if (m_videoButtonList)
        m_videoButtonList->Reset();

    QList<MythGenericTree*> *children = m_currentNode->getAllChildren();
    MythGenericTree *node;
    MythGenericTree *selectedNode = m_currentNode->getSelectedChild();
    QListIterator<MythGenericTree*> it(*children);
    while ( it.hasNext() )
    {
        node = it.next();
        if (!node)
            return;

        int nodeInt = node->getInt();

        Metadata *metadata = NULL;

        if (nodeInt >= 0)
            metadata = m_videoList->getVideoListMetadata(nodeInt);

        // Masking videos from a different parental level saves us have to
        // rebuild the tree
//         if (metadata &&
//                 metadata->ShowLevel() > m_currentParentalLevel->GetLevel())
//             continue;

        QString text = "";

        MythUIButtonListItem* item =
            new MythUIButtonListItem(m_videoButtonList, text, 0, true,
                                    MythUIButtonListItem::NotChecked);
        item->SetData(qVariantFromValue(node));

        UpdateItem(item);

        if (node == selectedNode)
            m_videoButtonList->SetItemCurrent(item);
    }
}

void VideoDialog::UpdateItem(MythUIButtonListItem *item)
{
    if (!item)
        item = GetItemCurrent();

    if (!item)
        return;

    MythGenericTree *node = qVariantValue<MythGenericTree*>(item->GetData());
    QString text = "";

    Metadata *metadata = GetMetadata(item);

    if (metadata)
        text = metadata->Title();
    else
        text = node->getString();

    item->setText(text);

    MythImage *img = GetCoverImage(node);

    if (img && !img->isNull())
        item->setImage(img);

    int nodeInt = node->getInt();
    if (nodeInt == kSubFolder)
        item->setText(QString("%1").arg(node->childCount()-1), "childcount");

    if (metadata)
    {
        item->setText(metadata->Director(), "director");
        item->setText(metadata->Plot(), "plot");
        item->setText(GetCast(*metadata), "cast");
        item->setText(getDisplayRating(metadata->Rating()), "rating");
        item->setText(metadata->InetRef(), "inetref");
        item->setText(getDisplayYear(metadata->Year()), "year");
        item->setText(getDisplayUserRating(metadata->UserRating()), "userrating");
        item->setText(getDisplayLength(metadata->Length()), "length");
        item->setText(metadata->CoverFile(), "coverfile");
        item->setText(metadata->Filename(), "filename");
        item->setText(QString::number(metadata->ChildID()), "child_id");
        item->setText(getDisplayBrowse(metadata->Browse()), "browseable");
        item->setText(metadata->Category(), "category");
        // TODO Add StateType support to mythuibuttonlist, then switch
        item->setText(QString::number(metadata->ShowLevel()), "videolevel");
    }

    if (item == GetItemCurrent())
        UpdateText(item);
}

void VideoDialog::fetchVideos()
{
    MythGenericTree *oldroot = m_rootNode;
    if (!m_treeLoaded)
    {
        m_rootNode = m_videoList->buildVideoList(m_isFileBrowser, m_isFlatList,
                                                *m_currentParentalLevel, true);
    }
    else
    {
        m_videoList->refreshList(m_isFileBrowser, *m_currentParentalLevel,
                                 m_isFlatList);
        m_rootNode = m_videoList->GetTreeRoot();
    }

    m_treeLoaded = true;

    m_rootNode->setOrderingIndex(kNodeSort);

    // Move a node down if there is a single directory item here...
    if (m_rootNode->childCount() == 1)
    {
        MythGenericTree *node = m_rootNode->getChildAt(0);
        if (node->getInt() == kSubFolder && node->childCount() > 1)
            m_rootNode = node;
        else
            m_treeLoaded = false;
    }
    else if (m_rootNode->childCount() == 0)
        m_treeLoaded = false;

    if (!m_currentNode || m_rootNode != oldroot)
        SetCurrentNode(m_rootNode);
}

MythImage* VideoDialog::GetCoverImage(MythGenericTree *node)
{
    int nodeInt = node->getInt();

    QString icon_file = "";

    if (nodeInt  == kSubFolder)  // subdirectory
    {
        // load folder icon
        int folder_id = node->getAttribute(kFolderPath);
        QString folder_path = m_videoList->getFolderPath(folder_id);

        QString filename = QString("%1/folder").arg(folder_path);

        QStringList test_files;
        test_files.append(filename + ".png");
        test_files.append(filename + ".jpg");
        test_files.append(filename + ".gif");
        for (QStringList::const_iterator tfp = test_files.begin();
                tfp != test_files.end(); ++tfp)
        {
            if (QFile::exists(*tfp))
            {
                icon_file = *tfp;
                break;
            }
        }

        // If we found nothing, load the first image we find
        if (icon_file.isEmpty())
        {
            QDir vidDir(folder_path);
            QStringList imageTypes;

            imageTypes.append("*.jpg");
            imageTypes.append("*.png");
            imageTypes.append("*.gif");
            vidDir.setNameFilters(imageTypes);

            QStringList fList = vidDir.entryList();
            if (!fList.isEmpty())
            {
                icon_file = QString("%1/%2")
                                    .arg(folder_path)
                                    .arg(fList.at(0));

                VERBOSE(VB_GENERAL,"Found Image : " << icon_file);
            }
        }

        if (icon_file.isEmpty())
            icon_file = "mv_gallery_dir.png";
    }
    else if (nodeInt == kUpFolder) // up-directory
    {
        icon_file = "mv_gallery_dir_up.png";

//         // prime the cache
//         if (!ImageCache::getImageCache().hitTest(icon_file))
//         {
//             std::auto_ptr<QImage> image(GetMythUI()->
//                                         LoadScaleImage(icon_file));
//             if (image.get())
//             {
//                 QPixmap pm(*image.get());
//                 ImageCache::getImageCache().load(icon_file, &pm);
//             }
//         }
    }
    else
    {
        Metadata *metadata = NULL;
        metadata = m_videoList->getVideoListMetadata(nodeInt);

        // load video icon
        if (metadata)
            icon_file = metadata->CoverFile();
    }

    MythImage *image = NULL;

    if (!icon_file.isEmpty() && icon_file != "No Cover")
    {
        image = GetMythPainter()->GetFormatImage();
        image->Load(icon_file);
    }

    return image;
}

bool VideoDialog::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Video", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "INFO")
        {
            if (!m_menuPopup && m_currentNode->getInt() > kSubFolder)
                InfoMenu();
        }
        else if (action == "INCPARENT")
            shiftParental(1);
        else if (action == "DECPARENT")
            shiftParental(-1);
        else if (action == "1" || action == "2" ||
                 action == "3" || action == "4")
            setParentalLevel((ParentalLevel::Level)action.toInt());
        else if (action == "FILTER")
            ChangeFilter();
        else if (action == "MENU")
        {
            if (!m_menuPopup)
                VideoMenu();
        }
        else if (action == "ESCAPE")
        {
            if ((m_type != DLG_TREE) && m_currentNode != m_rootNode)
                handled = goBack();
            else
                handled = false;
        }
        else
            handled = false;
    }

    if (!handled)
    {
        gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", event,
                                                     actions);

        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "PLAYBACK")
            {
                handled = true;
                playVideo();
            }
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void VideoDialog::createBusyDialog(const QString &title)
{
    if (m_busyPopup)
        return;

    QString message = title;

    m_busyPopup = new MythUIBusyDialog(message, m_popupStack, "mythvideobusydialog");

    if (m_busyPopup->Create())
        m_popupStack->AddScreen(m_busyPopup, false);
}

void VideoDialog::createOkDialog(const QString &title)
{
    QString message = title;

    MythConfirmationDialog *okPopup = new MythConfirmationDialog(m_popupStack, message, false);

    if (okPopup->Create())
        m_popupStack->AddScreen(okPopup, false);
}

bool VideoDialog::goBack()
{
    bool handled = false;

//     if (m_parent->IsExitingToMain())
//         return handled;

    if (m_currentNode != m_rootNode)
    {
        // one dir up
        MythGenericTree *lparent = m_currentNode->getParent();
        if (lparent)
        {
            // move one node up in the video tree
            SetCurrentNode(lparent);

            handled = true;
        }
    }

    loadData();
    return handled;
}

void VideoDialog::SetCurrentNode(MythGenericTree *node)
{
    if (!node)
        return;

    m_currentNode = node;
}

void VideoDialog::UpdateText(MythUIButtonListItem* item)
{
    if (!item)
        return;

    MythUIButtonList *currentList = item->parent();

    if (!currentList)
        return;

    Metadata *metadata = GetMetadata(item);

    if (m_titleText)
    {
        if (metadata)
            m_titleText->SetText(metadata->Title());
        else
            m_titleText->SetText(item->text());
    }

    if (m_positionText)
    {
        QString position = QString(tr("%1 of %2"))
                                .arg(currentList->GetCurrentPos() + 1)
                                .arg(currentList->GetCount());
        m_positionText->SetText(position);
    }

    if (m_crumbText)
    {
        QStringList route = m_currentNode->getRouteByString();
        //route.removeLast();
        m_crumbText->SetText(route.join(" > "));
    }

    MythGenericTree *node = qVariantValue<MythGenericTree*>(item->GetData());

    if (metadata)
    {
        item->setText(metadata->Filename(), "filename");
        item->setText("video_player", Metadata::getPlayer(metadata));

        QString coverfile = metadata->CoverFile();

        if (m_coverImage)
        {
            if (coverfile != "No Cover")
            {
                m_coverImage->SetFilename(coverfile);
                m_coverImage->Load();
            }
            else
                m_coverImage->Reset();
        }

        SetWidgetText(metadata->Director(), "director");
        SetWidgetText(metadata->Plot(), "plot");
        SetWidgetText(GetCast(*metadata), "cast");
        SetWidgetText(getDisplayRating(metadata->Rating()), "rating");
        SetWidgetText(metadata->InetRef(), "inetref");
        SetWidgetText(getDisplayYear(metadata->Year()), "year");
        SetWidgetText(getDisplayUserRating(metadata->UserRating()), "userrating");
        SetWidgetText(getDisplayLength(metadata->Length()), "length");
        SetWidgetText(metadata->Filename(), "filename");
        SetWidgetText(metadata->PlayCommand(), "player");
        SetWidgetText(metadata->CoverFile(), "coverfile");
        SetWidgetText(QString::number(metadata->ChildID()), "child_id");
        SetWidgetText(getDisplayBrowse(metadata->Browse()), "browseable");
        SetWidgetText(metadata->Category(), "category");
        if (m_videoLevelState)
        {
            QString statename = "";
            switch (metadata->ShowLevel())
            {
                case ParentalLevel::plLowest :
                    statename = "Lowest";
                    break;
                case ParentalLevel::plLow :
                    statename = "Low";
                    break;
                case ParentalLevel::plMedium :
                    statename = "Medium";
                    break;
                case ParentalLevel::plHigh :
                    statename = "High";
                    break;
                default :
                    statename = "None";
            }
            m_videoLevelState->DisplayState(statename);
        }
        SetWidgetText("", "childcount");
    }
    else
    {
        if (m_coverImage)
            m_coverImage->Reset();

        if (node && node->getInt() == kSubFolder)
            SetWidgetText(QString("%1").arg(node->childCount()-1), "childcount");

        SetWidgetText("", "director");
        SetWidgetText("", "plot");
        SetWidgetText("", "cast");
        SetWidgetText("", "rating");
        SetWidgetText("", "inetref");
        SetWidgetText("", "year");
        SetWidgetText("", "userrating");
        SetWidgetText("", "length");
        SetWidgetText("", "filename");
        SetWidgetText("", "player");
        SetWidgetText("", "coverfile");
        SetWidgetText("", "child_id");
        SetWidgetText("", "browseable");
        SetWidgetText("", "category");
    }

    if (node)
        node->becomeSelectedChild();
}

void VideoDialog::SetWidgetText(QString value, QString widgetname)
{
    MythUIText *text = dynamic_cast<MythUIText *> (GetChild(widgetname));
    if (text)
        text->SetText(value);
}

void VideoDialog::VideoMenu()
{
    QString label = tr("Select action");

    m_menuPopup = new MythDialogBox(label, m_popupStack, "videomenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup, false);

    m_menuPopup->SetReturnEvent(this, "actions");

    MythUIButtonListItem *item = GetItemCurrent();
    if (item)
    {
        MythGenericTree *node = qVariantValue<MythGenericTree*>(item->GetData());
        if (node && node->getInt() >= 0)
        {
            m_menuPopup->AddButton(tr("Watch This Video"), SLOT(playVideo()));
            m_menuPopup->AddButton(tr("Video Info"), SLOT(InfoMenu()));
            m_menuPopup->AddButton(tr("Manage Video"), SLOT(ManageMenu()));
        }
    }
    m_menuPopup->AddButton(tr("Scan For Changes"), SLOT(doVideoScan()));
    m_menuPopup->AddButton(tr("Change View"), SLOT(ViewMenu()));
    m_menuPopup->AddButton(tr("Filter Display"), SLOT(ChangeFilter()));

    m_menuPopup->AddButton(tr("Cancel"));
}

void VideoDialog::ViewMenu()
{
    QString label = tr("Change View");

    m_menuPopup = new MythDialogBox(label, m_popupStack, "videomenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup, false);

    m_menuPopup->SetReturnEvent(this, "view");

    if (!(m_type & DLG_BROWSER))
        m_menuPopup->AddButton(tr("Switch to Browse View"), SLOT(SwitchBrowse()));

    if (!(m_type & DLG_GALLERY))
        m_menuPopup->AddButton(tr("Switch to Gallery View"), SLOT(SwitchGallery()));

    if (!(m_type & DLG_TREE))
        m_menuPopup->AddButton(tr("Switch to List View"), SLOT(SwitchTree()));

    if (!(m_type & DLG_MANAGER))
        m_menuPopup->AddButton(tr("Switch to Manage View"), SLOT(SwitchManager()));


    if (m_isFileBrowser)
        m_menuPopup->AddButton(tr("Disable File Browse Mode"),
                                                    SLOT(ToggleBrowseMode()));
    else
        m_menuPopup->AddButton(tr("Enable File Browse Mode"),
                                                    SLOT(ToggleBrowseMode()));

    if (m_isFlatList)
        m_menuPopup->AddButton(tr("Disable Flat View"),
                                                    SLOT(ToggleFlatView()));
    else
        m_menuPopup->AddButton(tr("Enable Flat View"),
                                                    SLOT(ToggleFlatView()));

    m_menuPopup->AddButton(tr("Cancel"));
}

void VideoDialog::InfoMenu()
{
    QString label = tr("Select action");

    m_menuPopup = new MythDialogBox(label, m_popupStack, "videomenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup, false);

    m_menuPopup->SetReturnEvent(this, "info");

    m_menuPopup->AddButton(tr("View Full Plot"), SLOT(ViewPlot()));
    m_menuPopup->AddButton(tr("View Cast"), SLOT(ShowCastDialog()));

    m_menuPopup->AddButton(tr("Cancel"));
}

void VideoDialog::ManageMenu()
{
    QString label = tr("Select action");

    m_menuPopup = new MythDialogBox(label, m_popupStack, "videomenupopup");

    if (m_menuPopup->Create())
        m_popupStack->AddScreen(m_menuPopup, false);

    m_menuPopup->SetReturnEvent(this, "manage");

    m_menuPopup->AddButton(tr("Edit Metadata"), SLOT(EditMetadata()));
    m_menuPopup->AddButton(tr("Download Metadata"), SLOT(VideoSearch()));
    m_menuPopup->AddButton(tr("Manually Enter Video #"), SLOT(ManualVideoUID()));
    m_menuPopup->AddButton(tr("Manually Enter Video Title"), SLOT(ManualVideoTitle()));
    m_menuPopup->AddButton(tr("Reset Metadata"), SLOT(ResetMetadata()));
    m_menuPopup->AddButton(tr("Toggle Browseable"), SLOT(ToggleBrowseable()));
    m_menuPopup->AddButton(tr("Remove Video"), SLOT(RemoveVideo()));

    m_menuPopup->AddButton(tr("Cancel"));
}

void VideoDialog::ToggleBrowseMode()
{
    m_isFileBrowser = !m_isFileBrowser;
    gContext->SetSetting("VideoDialogNoDB", QString("%1").arg((int)m_isFileBrowser));
    reloadData();
}

void VideoDialog::ToggleFlatView()
{
    m_isFlatList = !m_isFlatList;
    gContext->SetSetting(QString("mythvideo.folder_view_%1").arg(m_type),
                         QString("%1").arg((int)m_isFlatList));
    // TODO: This forces a complete tree rebuild, this is SLOW and shouldn't
    // be necessary since MythGenericTree can do a flat view without a rebuild,
    // I just don't want to re-write VideoList just now
    reloadData();
}

void VideoDialog::handleDirSelect(MythGenericTree *node)
{
    SetCurrentNode(node);
    loadData();
}

void VideoDialog::handleSelect(MythUIButtonListItem *item)
{
    MythGenericTree *node = qVariantValue<MythGenericTree*>(item->GetData());
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
            playVideo();
    };
}

void VideoDialog::SwitchTree()
{
    SwitchLayout(DLG_TREE);
}

void VideoDialog::SwitchGallery()
{
    SwitchLayout(DLG_GALLERY);
}

void VideoDialog::SwitchBrowse()
{
    SwitchLayout(DLG_BROWSER);
}

void VideoDialog::SwitchManager()
{
    SwitchLayout(DLG_MANAGER);
}

void VideoDialog::SwitchLayout(DialogType type)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    VideoDialog *mythvideo;

    if (type == DLG_TREE)
        mythvideo = new VideoTree(mainStack, "mythvideo", m_videoList, type);
    else
        mythvideo = new VideoDialog(mainStack, "mythvideo", m_videoList, type);

    if (mythvideo->Create())
    {
        GetScreenStack()->PopScreen(false);
        GetScreenStack()->AddScreen(mythvideo, false);
    }
}

void VideoDialog::ViewPlot()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());

    PlotDialog *plotdialog = new PlotDialog(m_popupStack, metadata);

    if (plotdialog->Create())
        m_popupStack->AddScreen(plotdialog);
}

void VideoDialog::ShowCastDialog()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());

    CastDialog *castdialog = new CastDialog(m_popupStack, metadata);

    if (castdialog->Create())
        m_popupStack->AddScreen(castdialog);
}

void VideoDialog::playVideo()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());

    PlayVideo(metadata->Filename(), m_videoList->getListCache());

    gContext->GetMainWindow()->raise();
    gContext->GetMainWindow()->activateWindow();
    if (gContext->GetMainWindow()->currentWidget())
        gContext->GetMainWindow()->currentWidget()->setFocus();
}

void VideoDialog::setParentalLevel(const ParentalLevel::Level &level)
{
    ParentalLevel::Level new_level = level;
    if (new_level == ParentalLevel::plNone)
        new_level = ParentalLevel::plLowest;

    m_currentParentalLevel->SetLevel(new_level);
}

void VideoDialog::shiftParental(int amount)
{
    int newlevel = (int)m_currentParentalLevel->GetLevel() + amount;
    if (newlevel <= ParentalLevel::plHigh)
        setParentalLevel((ParentalLevel::Level)newlevel);
}

void VideoDialog::ChangeFilter()
{
    MythScreenStack *mainStack = GetScreenStack();

    VideoFilterDialog *filterdialog = new VideoFilterDialog(mainStack,
                                                            "videodialogfilters",
                                                            m_videoList);

    if (filterdialog->Create())
        mainStack->AddScreen(filterdialog);

    connect(filterdialog, SIGNAL(filterChanged()), SLOT(reloadData()));
}

void VideoDialog::customEvent(QEvent *event)
{
    if (event->type() == kMythDialogBoxCompletionEventType)
    {
        m_menuPopup = NULL;
    }
}

Metadata* VideoDialog::GetMetadata(MythUIButtonListItem *item)
{
    Metadata *metadata = NULL;

    if (item)
    {
        MythGenericTree *node = qVariantValue<MythGenericTree*>(item->GetData());
        if (node)
        {
            int nodeInt = node->getInt();

            if (nodeInt >= 0)
                metadata = m_videoList->getVideoListMetadata(nodeInt);
        }
    }

    return metadata;
}

MythUIButtonListItem *VideoDialog::GetItemCurrent()
{
    return m_videoButtonList->GetItemCurrent();
}

void VideoDialog::VideoSearch()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());

    if (metadata)
        StartVideoSearchByTitle(metadata->InetRef(), metadata->Title(),
                                metadata);
}

void VideoDialog::ToggleBrowseable()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (metadata)
    {
        metadata->setBrowse(!metadata->Browse());
        metadata->updateDatabase();

        refreshData();
    }
}

// void VideoDialog::OnVideoSearchListCancel()
// {
//     // I'm keeping this behavior for now, though item
//     // modification on Cancel is seems anathema to me.
//     Metadata *item = GetItemCurrent();
//
//     if (item && isDefaultCoverFile(item->CoverFile()))
//     {
//         QStringList search_dirs;
//         search_dirs += m_artDir;
//         QString cover_file;
//
//         if (GetLocalVideoPoster(item->InetRef(), item->Filename(),
//                                 search_dirs, cover_file))
//         {
//             item->setCoverFile(cover_file);
//             item->updateDatabase();
//             loadData();
//         }
//     }
// }

void VideoDialog::OnVideoSearchListSelection(const QString &video_uid)
{
    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (metadata && !video_uid.isEmpty())
    {
        StartVideoSearchByUID(video_uid, metadata);
    }
}

// void VideoDialog::RefreshVideoList(bool resort_only)
// {
//     static bool updateML = false;
//     if (updateML == true)
//         return;
//     updateML = true;
//
//     unsigned int selected_id = 0;
//     const Metadata *metadata = GetMetadata(GetItemCurrent());
//     if (metadata)
//         selected_id = metadata->ID();
//
//     if (resort_only)
//     {
//         m_videoList->resortList(true);
//     }
//     else
//     {
//         m_videoList->refreshList(false,
//                 ParentalLevel(ParentalLevel::plNone), true);
//     }
//
//     m_videoButtonList->OnListChanged();
//
//     // TODO: This isn't perfect, if you delete the last item your selection
//     // reverts to the first item.
//     if (selected_id)
//     {
//         MetadataListManager::MetadataPtr sel_item =
//                 m_videoList->getListCache().byID(selected_id);
//         if (sel_item)
//         {
//             m_videoButtonList->SetSelectedItem(sel_item->getFlatIndex());
//         }
//     }
//
//     updateML = false;
// }

void VideoDialog::AutomaticParentalAdjustment(Metadata *metadata)
{
    if (metadata && m_rating_to_pl.size())
    {
        QString rating = metadata->Rating();
        for (parental_level_map::const_iterator p = m_rating_to_pl.begin();
            rating.length() && p != m_rating_to_pl.end(); ++p)
        {
            if (rating.find(p->first) != -1)
            {
                metadata->setShowLevel(p->second);
                break;
            }
        }
    }
}

void VideoDialog::OnParentalChange(int amount)
{
    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (metadata)
    {
        ParentalLevel curshowlevel = metadata->ShowLevel();

        curshowlevel += amount;

        if (curshowlevel.GetLevel() != metadata->ShowLevel())
        {
            metadata->setShowLevel(curshowlevel.GetLevel());
            metadata->updateDatabase();
            refreshData();
        }
    }
}

void VideoDialog::ManualVideoUID()
{
    QString message = tr("Enter Video Unique ID:");

    MythTextInputDialog *searchdialog =
                                new MythTextInputDialog(m_popupStack,message);

    if (searchdialog->Create())
        m_popupStack->AddScreen(searchdialog);

    connect(searchdialog, SIGNAL(haveResult(QString &)),
            SLOT(OnManualVideoUID(QString)), Qt::QueuedConnection);
}

void VideoDialog::OnManualVideoUID(const QString &video_uid)
{
    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (video_uid.length())
        StartVideoSearchByUID(video_uid, metadata);
}

void VideoDialog::ManualVideoTitle()
{
    QString message = tr("Enter Video Title:");

    MythTextInputDialog *searchdialog =
                                new MythTextInputDialog(m_popupStack,message);

    if (searchdialog->Create())
        m_popupStack->AddScreen(searchdialog);

    connect(searchdialog, SIGNAL(haveResult(QString)),
            SLOT(OnManualVideoTitle(QString)), Qt::QueuedConnection);
}

void VideoDialog::OnManualVideoTitle(const QString &title)
{
    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (title.length() && metadata)
    {
        StartVideoSearchByTitle(VIDEO_INETREF_DEFAULT, title, metadata);
    }
}

void VideoDialog::EditMetadata()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());
    if (!metadata)
        return;

    MythScreenStack *screenStack = GetScreenStack();

    EditMetadataDialog *md_editor = new EditMetadataDialog(screenStack,
            "mythvideoeditmetadata",metadata, m_videoList->getListCache());

    connect(md_editor, SIGNAL(Finished()), SLOT(refreshData()));

    if (md_editor->Create())
        screenStack->AddScreen(md_editor);
}

void VideoDialog::RemoveVideo()
{
    Metadata *metadata = GetMetadata(GetItemCurrent());

    if (!metadata)
        return;

    QString message = tr("Delete this file?");

    MythConfirmationDialog *confirmdialog =
                                new MythConfirmationDialog(m_popupStack,message);

    if (confirmdialog->Create())
        m_popupStack->AddScreen(confirmdialog);

    connect(confirmdialog, SIGNAL(haveResult(bool)),
            SLOT(OnRemoveVideo(bool)));
}

void VideoDialog::OnRemoveVideo(bool dodelete)
{
    if (!dodelete)
        return;

    Metadata *metadata = GetMetadata(GetItemCurrent());

    if (!metadata)
        return;

    if (m_videoList->Delete(metadata->ID()))
        refreshData();
    else
    {
        QString message = tr("Failed to delete file");

        MythConfirmationDialog *confirmdialog =
                        new MythConfirmationDialog(m_popupStack,message,false);

        if (confirmdialog->Create())
            m_popupStack->AddScreen(confirmdialog);
    }
}

void VideoDialog::ResetMetadata()
{
    MythUIButtonListItem *item = GetItemCurrent();
    Metadata *metadata = GetMetadata(item);

    if (metadata)
    {
        ResetItem(metadata);

        QString cover_file;
        QStringList search_dirs;
        search_dirs += m_artDir;
        if (GetLocalVideoPoster(metadata->InetRef(), metadata->Filename(),
                        search_dirs, cover_file))
        {
            metadata->setCoverFile(cover_file);
            metadata->updateDatabase();
            UpdateItem(item);
        }
    }
}

void VideoDialog::ResetItem(Metadata *metadata)
{
    if (metadata)
    {
        metadata->Reset();
        metadata->updateDatabase();

        UpdateItem(GetItemCurrent());
    }
}

//
// Possibly move the following elsewhere, e.g. video utils
//

// Copy video poster to appropriate directory and set the item's cover file.
// This is the start of an async operation that needs to always complete
// to OnVideoPosterSetDone.
void VideoDialog::StartVideoPosterSet(Metadata *metadata)
{
    //createBusyDialog(QObject::tr("Fetching poster for %1 (%2)")
    //                    .arg(metadata->InetRef())
    //                    .arg(metadata->Title()));
    QStringList search_dirs;
    search_dirs += m_artDir;

    QString cover_file;

    if (GetLocalVideoPoster(metadata->InetRef(), metadata->Filename(),
                            search_dirs, cover_file))
    {
        metadata->setCoverFile(cover_file);
        OnVideoPosterSetDone(metadata);
        return;
    }

    // Obtain video poster
    VideoPosterSearch *vps = new VideoPosterSearch(this);
    connect(vps, SIGNAL(SigPosterURL(const QString &, Metadata *)),
            SLOT(OnPosterURL(const QString &, Metadata *)));
    vps->Run(metadata->InetRef(), metadata);
}

void VideoDialog::OnPosterURL(const QString &uri, Metadata *metadata)
{
    if (metadata)
    {
        if (uri.length())
        {
            QString fileprefix = m_artDir;

            QDir dir;

            // If the video artwork setting hasn't been set default to
            // using ~/.mythtv/MythVideo
            if (fileprefix.length() == 0)
            {
                fileprefix = GetConfDir();

                dir.setPath(fileprefix);
                if (!dir.exists())
                    dir.mkdir(fileprefix);

                fileprefix += "/MythVideo";
            }

            dir.setPath(fileprefix);
            if (!dir.exists())
                dir.mkdir(fileprefix);

            Q3Url url(uri);

            QString ext = QFileInfo(url.fileName()).extension(false);
            QString dest_file = QString("%1/%2.%3").arg(fileprefix)
                    .arg(metadata->InetRef()).arg(ext);
            VERBOSE(VB_IMPORTANT, QString("Copying '%1' -> '%2'...")
                    .arg(uri).arg(dest_file));

            metadata->setCoverFile(dest_file);

            m_url_operator.copy(uri, QString("file:%1").arg(dest_file),
                                metadata);
            VERBOSE(VB_IMPORTANT,
                    QString("dest_file = %1").arg(dest_file));

            const int nTimeout =
                    gContext->GetNumSetting("PosterDownloadTimeout", 30)
                    * 1000;
            m_url_dl_timer.start(nTimeout, metadata, url);
        }
        else
        {
            metadata->setCoverFile("");
            OnVideoPosterSetDone(metadata);
        }
    }
    else
        OnVideoPosterSetDone(metadata);
}

void VideoDialog::OnPosterCopyFinished(Q3NetworkOperation *op,
                                        Metadata *item)
{
    m_url_dl_timer.stop();
    QString state, operation;
    switch(op->operation())
    {
        case Q3NetworkProtocol::OpMkDir:
            operation = "MkDir";
            break;
        case Q3NetworkProtocol::OpRemove:
            operation = "Remove";
            break;
        case Q3NetworkProtocol::OpRename:
            operation = "Rename";
            break;
        case Q3NetworkProtocol::OpGet:
            operation = "Get";
            break;
        case Q3NetworkProtocol::OpPut:
            operation = "Put";
            break;
        default:
            operation = "Uknown";
            break;
    }

    switch(op->state())
    {
        case Q3NetworkProtocol::StWaiting:
            state = "The operation is in the QNetworkProtocol's queue "
                    "waiting to be prcessed.";
            break;
        case Q3NetworkProtocol::StInProgress:
            state = "The operation is being processed.";
            break;
        case Q3NetworkProtocol::StDone:
            state = "The operation has been processed succesfully.";
            break;
        case Q3NetworkProtocol::StFailed:
            state = "The operation has been processed but an error "
                    "occurred.";
            if (item)
                item->setCoverFile("");
            break;
        case Q3NetworkProtocol::StStopped:
            state = "The operation has been processed but has been stopped "
                    "before it finished, and is waiting to be processed.";
            break;
        default:
            state = "Unknown";
            break;
    }

    VERBOSE(VB_IMPORTANT, QString("%1: %2: %3").arg(operation).arg(state)
            .arg(op->protocolDetail()));

    OnVideoPosterSetDone(item);
}

void VideoDialog::OnPosterDownloadTimeout(const QString &url,
                                        Metadata *item)
{
    VERBOSE(VB_IMPORTANT, QString("Copying of '%1' timed out").arg(url));

    if (item)
        item->setCoverFile("");

    m_url_operator.stop(); // results in OnPosterCopyFinished

    createOkDialog(tr("A poster exists for this item but could not be "
                        "retrieved within the timeout period.\n"));
}

// This is the final call as part of a StartVideoPosterSet
void VideoDialog::OnVideoPosterSetDone(Metadata *metadata)
{
    // The metadata has some cover file set
    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = NULL;
    }

    metadata->updateDatabase();
    UpdateItem(GetItemCurrent());
}

void VideoDialog::StartVideoSearchByUID(const QString &video_uid,
                                            Metadata *metadata)
{
    // Starting the busy dialog here triggers a bizarre segfault
    //createBusyDialog(video_uid);
    VideoUIDSearch *vns = new VideoUIDSearch(this);
    connect(vns, SIGNAL(SigSearchResults(bool, const QStringList &,
                                        Metadata *, const QString &)),
            SLOT(OnVideoSearchByUIDDone(bool, const QStringList &,
                                        Metadata *, const QString &)));
    vns->Run(video_uid, metadata);
}

void VideoDialog::OnVideoSearchByUIDDone(bool normal_exit,
                                            const QStringList &output,
                                            Metadata *metadata,
                                            const QString &video_uid)
{
    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = NULL;
    }

    std::map<QString, QString> data;

    if (normal_exit && output.size())
    {
        for (QStringList::const_iterator p = output.begin();
            p != output.end(); ++p)
        {
            data[(*p).section(':', 0, 0)] = (*p).section(':', 1);
        }
        // set known values
        metadata->setTitle(data["Title"]);
        metadata->setYear(data["Year"].toInt());
        metadata->setDirector(data["Director"]);
        metadata->setPlot(data["Plot"]);
        metadata->setUserRating(data["UserRating"].toFloat());
        metadata->setRating(data["MovieRating"]);
        metadata->setLength(data["Runtime"].toInt());

        AutomaticParentalAdjustment(metadata);

        // Cast
        Metadata::cast_list cast;
        QStringList cl = data["Cast"].split(",", QString::SkipEmptyParts);

        for (QStringList::const_iterator p = cl.begin();
            p != cl.end(); ++p)
        {
            QString cn = (*p).trimmed();
            if (cn.length())
            {
                cast.push_back(Metadata::cast_list::
                            value_type(-1, cn));
            }
        }

        metadata->setCast(cast);

        // Genres
        Metadata::genre_list video_genres;
        QStringList genres = data["Genres"].split(",", QString::SkipEmptyParts);

        for (QStringList::const_iterator p = genres.begin();
            p != genres.end(); ++p)
        {
            QString genre_name = (*p).trimmed();
            if (genre_name.length())
            {
                video_genres.push_back(
                        Metadata::genre_list::value_type(-1, genre_name));
            }
        }

        metadata->setGenres(video_genres);

        // Countries
        Metadata::country_list video_countries;
        QStringList countries = data["Countries"].split(",", QString::SkipEmptyParts);
        for (QStringList::const_iterator p = countries.begin();
            p != countries.end(); ++p)
        {
            QString country_name = (*p).trimmed();
            if (country_name.length())
            {
                video_countries.push_back(
                        Metadata::country_list::value_type(-1,
                                country_name));
            }
        }

        metadata->setCountries(video_countries);

        metadata->setInetRef(video_uid);
        StartVideoPosterSet(metadata);
    }
    else
    {
        ResetItem(metadata);
        metadata->updateDatabase();
        UpdateItem(GetItemCurrent());
    }
}

void VideoDialog::StartVideoSearchByTitle(const QString &video_uid,
                                            const QString &title,
                                            Metadata *metadata)
{
    if (video_uid == VIDEO_INETREF_DEFAULT)
    {
        createBusyDialog(title);

        VideoTitleSearch *vts = new VideoTitleSearch(this);
        connect(vts,
                SIGNAL(SigSearchResults(bool,
                        const SearchListResults &, Metadata *)),
                SLOT(OnVideoSearchByTitleDone(bool,
                        const SearchListResults &, Metadata *)));
        vts->Run(title, metadata);
    }
    else
    {
        SearchListResults videos;
        videos.insertMulti(video_uid, title);
        OnVideoSearchByTitleDone(true, videos, metadata);
    }
}

void VideoDialog::OnVideoSearchByTitleDone(bool normal_exit,
        const SearchListResults &results, Metadata *metadata)
{
    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = NULL;
    }

    (void) normal_exit;
    VERBOSE(VB_IMPORTANT,
            QString("GetVideoList returned %1 possible matches")
            .arg(results.size()));

    if (results.size() == 1)
    {
        // Only one search result, fetch data.
        if (results.begin().value().isEmpty())
        {
            ResetItem(metadata);
            return;
        }
        StartVideoSearchByUID(results.begin().key(), metadata);
    }
    else
    {
        SearchResultsDialog *resultsdialog = new SearchResultsDialog(m_popupStack,
                                                                     results);

        if (resultsdialog->Create())
            m_popupStack->AddScreen(resultsdialog);

        connect(resultsdialog, SIGNAL(haveResult(QString)),
                SLOT(OnVideoSearchListSelection(QString)),
                Qt::QueuedConnection);
    }
}

void VideoDialog::doVideoScan()
{
    if (!m_scanner)
        m_scanner = new VideoScanner();
    connect(m_scanner, SIGNAL(finished()), SLOT(reloadData()));
    m_scanner->doScan(GetVideoDirs());
}
