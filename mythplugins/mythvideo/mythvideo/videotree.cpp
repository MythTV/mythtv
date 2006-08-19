#include <qapplication.h>

#include <stdlib.h>

#include <mythtv/mythcontext.h>
#include <mythtv/uitypes.h>

#include "globals.h"
#include "videotree.h"
#include "videofilter.h"
#include "videolist.h"
#include "metadata.h"
#include "videoutils.h"

class VideoTreeImp
{
  public:
    UIManagedTreeListType *video_tree_list;
    UITextType *video_title;
    UITextType *video_file;
    UITextType *video_plot;
    UITextType *video_player;
    UITextType *pl_value;
    UIImageType *video_poster;

    UITextType *m_director;
    UITextType *m_rating;
    UITextType *m_inetref;
    UITextType *m_year;
    UITextType *m_userrating;
    UITextType *m_length;
    UITextType *m_coverfile;
    UITextType *m_child_id;
    UITextType *m_browseable;
    UITextType *m_category;
    UITextType *m_level;

    bool m_use_arrow_accel;

    VideoTreeImp() :
        video_tree_list(NULL), video_title(NULL),
        video_file(NULL), video_plot(NULL), video_player(NULL),
        pl_value(NULL), video_poster(NULL), m_director(NULL), m_rating(NULL),
        m_inetref(NULL), m_year(NULL), m_userrating(NULL), m_length(NULL),
        m_coverfile(NULL), m_child_id(NULL), m_browseable(NULL),
        m_category(NULL), m_level(NULL)
    {
        m_use_arrow_accel = gContext->GetNumSetting("UseArrowAccels", 1);
    }

    void update_info(const Metadata *item)
    {
        if (item)
            update_screen(item);
        else
            reset_screen();
    }

    void wireUpTheme(VideoTree *vt)
    {
        //  Big tree thingy at the top
        assign(vt, video_tree_list, "videotreelist");
        if (!video_tree_list)
        {
            // TODO: bad
            exit(0);
        }
        video_tree_list->showWholeTree(true);
        video_tree_list->colorSelectables(true);

        // Text areas
        assign(vt, video_title, "video_title");
        assign(vt, video_file, "video_file");
        assign(vt, video_player, "video_player");

        // Status of Parental Level
        assign(vt, pl_value, "pl_value");

        // Cover Art
        assign(vt, video_poster, "video_poster");

        // Optional
        assign(vt, m_director, "director", false);
        assign(vt, video_plot, "plot", false);
        assign(vt, m_rating, "rating", false);
        assign(vt, m_inetref, "inetref", false);
        assign(vt, m_year, "year", false);
        assign(vt, m_userrating, "userrating", false);
        assign(vt, m_length, "length", false);
        assign(vt, m_coverfile, "coverfile", false);
        assign(vt, m_child_id, "child_id", false);
        assign(vt, m_browseable, "browseable", false);
        assign(vt, m_category, "category", false);
        assign(vt, m_level, "level", false);
    }

  private:
    void reset_screen()
    {
        checkedSetText(video_title, "");
        checkedSetText(video_file, "");
        checkedSetText(video_player, "");

        if (video_poster)
            video_poster->ResetImage();

        checkedSetText(m_director, "");
        checkedSetText(video_plot, "");
        checkedSetText(m_rating, "");
        checkedSetText(m_inetref, "");
        checkedSetText(m_year, "");
        checkedSetText(m_userrating, "");
        checkedSetText(m_length, "");
        checkedSetText(m_coverfile, "");
        checkedSetText(m_child_id, "");
        checkedSetText(m_browseable, "");
        checkedSetText(m_category, "");
        checkedSetText(m_level, "");
    }

    void update_screen(const Metadata *item)
    {
        checkedSetText(video_title, item->Title());
        checkedSetText(video_file, item->Filename().section("/", -1));
        checkedSetText(video_plot, item->Plot());
        checkedSetText(video_player, Metadata::getPlayer(item));

        if (!isDefaultCoverFile(item->CoverFile()))
        {
            video_poster->SetImage(item->CoverFile());
        }
        else
        {
            video_poster->SetImage("blank.png");
        }
        video_poster->LoadImage();

        checkedSetText(m_director, item->Director());
        checkedSetText(m_rating, getDisplayRating(item->Rating()));
        checkedSetText(m_inetref, item->InetRef());
        checkedSetText(m_year, getDisplayYear(item->Year()));
        checkedSetText(m_userrating, getDisplayUserRating(item->UserRating()));
        checkedSetText(m_length, getDisplayLength(item->Length()));
        checkedSetText(m_coverfile, item->CoverFile());
        checkedSetText(m_child_id, QString::number(item->ChildID()));
        checkedSetText(m_browseable, getDisplayBrowse(item->Browse()));
        checkedSetText(m_category, item->Category());
        checkedSetText(m_level, QString::number(item->ShowLevel()));
    }

    template <typename T>
    void getType(VideoTree *vt, T *&value, const QString &name)
    {
        value = NULL;
        VERBOSE(VB_IMPORTANT, QString("Error: unknown theme element type"));
    }

    template <typename UIType>
    bool assign(VideoTree *vt, UIType *&type, const QString &name,
                bool warn = true)
    {

        getType<UIType>(vt, type, name);
        if (!type && warn)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("%1: Could not find theme element called %2 "
                            "in your theme").arg(__FILE__).arg(name));
        }
        return type != NULL;
    }
};

template <>
void VideoTreeImp::getType(VideoTree *vt, UIManagedTreeListType *&value,
             const QString &name)
{
    value = vt->getUIManagedTreeListType(name);
}

template <>
void VideoTreeImp::getType(VideoTree *vt, UITextType *&value,
                           const QString &name)
{
    value = vt->getUITextType(name);
}

template <>
void VideoTreeImp::getType(VideoTree *vt, UIImageType *&value,
                           const QString &name)
{
    value = vt->getUIImageType(name);
}

VideoTree::VideoTree(MythMainWindow *lparent, const QString &window_name,
                     const QString &theme_filename, const QString &lname,
                     VideoList *video_list) :
    MythThemedDialog(lparent, window_name, theme_filename, lname),
    popup(NULL), expectingPopup(false), curitem(NULL), m_video_list(video_list),
    video_tree_root(NULL), m_exit_type(0)
{
    m_imp.reset(new VideoTreeImp);

    current_parental_level =
            gContext->GetNumSetting("VideoDefaultParentalLevel", 1);

    file_browser = gContext->GetNumSetting("VideoTreeNoDB", 0);
    m_db_folders = gContext->GetNumSetting("mythvideo.db_folder_view", 1);

    // Theme and tree stuff
    m_imp->wireUpTheme(this);

    connect(m_imp->video_tree_list, SIGNAL(nodeSelected(int, IntVector*)), this,
                SLOT(handleTreeListSelection(int)));
    connect(m_imp->video_tree_list, SIGNAL(nodeEntered(int, IntVector*)), this,
                SLOT(handleTreeListEntry(int)));

    VideoFilterSettings video_filter(true, "VideoTree");
    m_video_list->setCurrentVideoFilter(video_filter);

    buildVideoList();
}

VideoTree::~VideoTree()
{
}

void VideoTree::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Video", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "SELECT")
            m_imp->video_tree_list->select();
        else if (action == "UP")
            m_imp->video_tree_list->moveUp();
        else if (action == "DOWN")
            m_imp->video_tree_list->moveDown();
        else if (action == "LEFT")
        {
            if (m_imp->video_tree_list->getCurrentNode()->getParent() !=
                video_tree_root)
            {
                m_imp->video_tree_list->popUp();
            }
            else
            {
                if (m_imp->m_use_arrow_accel)
                {
                    done(1);
                }
                else
                    handled = false;
            }
        }
        else if (action == "RIGHT")
            m_imp->video_tree_list->pushDown();
        else if (action == "PAGEUP")
            m_imp->video_tree_list->pageUp();
        else if (action == "PAGEDOWN")
            m_imp->video_tree_list->pageDown();
        else if (action == "INFO")
            doMenu(true);
        else if (action == "MENU")
            doMenu(false);
        else if (action == "INCPARENT")
            setParentalLevel(current_parental_level + 1);
        else if (action == "DECPARENT")
            setParentalLevel(current_parental_level - 1);
        else if (action == "1" || action == "2" || action == "3" ||
                 action == "4")
        {
            setParentalLevel(action.toInt());
        }
        else
            handled = false;
    }

    if (!handled)
    {
        gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e, actions);

        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "PLAYBACK")
            {
                handled = true;
                playVideo(curitem);
            }
        }
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void VideoTree::setParentalLevel(int which_level)
{
    if (which_level < 1)
    {
        which_level = 1;
    }

    if (which_level > 4)
    {
        which_level = 4;
    }

    if (checkParentPassword())
    {
        current_parental_level = which_level;
        checkedSetText(m_imp->pl_value,
                       QString::number(current_parental_level));
        buildVideoList();
    }
    else
    {
        checkedSetText(m_imp->pl_value,
                       QString::number(current_parental_level));
    }
}

void VideoTree::buildVideoList()
{
    video_tree_root = m_video_list->buildVideoList(file_browser, !m_db_folders,
                                                   current_parental_level,
                                                   false);
    GenericTree *video_tree_data = 0;

    if (video_tree_root->childCount() > 0)
        video_tree_data = video_tree_root->getChildAt(0,0);
    else
        video_tree_data = video_tree_root;

    m_imp->video_tree_list->assignTreeData(video_tree_root);

    //  Tell the tree list to highlight the
    //  first entry and then display it
    m_imp->video_tree_list->setCurrentNode(video_tree_data);
    if (video_tree_data->childCount() > 0)
    {
        m_imp->video_tree_list->
                setCurrentNode(video_tree_data->getChildAt(0, 0));
    }
    updateForeground();
    m_imp->video_tree_list->enter();
}

void VideoTree::handleTreeListEntry(int node_int)
{
    if (node_int > -1)
    {
        //  User has navigated to a video file

        curitem = m_video_list->getVideoListMetadata(node_int);
    }
    else
    {
        //  not a leaf node
        //  (no video file here, just a directory)
        curitem = NULL;
    }
    m_imp->update_info(curitem);

    // TODO: This update is excessive, we really just need
    // to cover the case where the new poster is smaller than the
    // previous one. Ideally the UI would invalidate our whole
    // staticsize automatically.
    updateForeground();
}

void VideoTree::playVideo(int node_number)
{
    if (node_number > -1)
    {
        playVideo(curitem);
    }
}

void VideoTree::handleTreeListSelection(int node_int)
{
    if (node_int > -1)
    {
        playVideo(node_int);
    }
}

bool VideoTree::createPopup()
{
    if (!popup)
    {
        //allowPaint = false;
        popup = new MythPopupBox(gContext->GetMainWindow(), "video popup");

        expectingPopup = true;

        popup->addLabel(tr("Select action"));
        popup->addLabel("");
    }

    return (popup != NULL);
}


void VideoTree::doMenu(bool info)
{
    if (createPopup())
    {
        QButton *focusButton = NULL;
        if (info)
        {
            focusButton = popup->addButton(tr("Watch This Video"), this,
                                           SLOT(slotWatchVideo()));
            popup->addButton(tr("View Full Plot"), this, SLOT(slotViewPlot()));
        }
        else
        {
            QButton *tempButton = NULL;
            if (!file_browser)
                focusButton = popup->addButton(tr("Filter Display"), this,
                                               SLOT(slotDoFilter()));

            tempButton = popup->addButton(tr("Switch to Browse View"), this,
                                          SLOT(slotVideoBrowser()));

            if (!focusButton)
                focusButton = tempButton;

            popup->addButton(tr("Switch to Gallery View"), this,
                             SLOT(slotVideoGallery()));
        }

        popup->addButton(tr("Cancel"), this, SLOT(slotDoCancel()));

        popup->ShowPopup(this, SLOT(slotDoCancel()));

        focusButton->setFocus();
    }
}

void VideoTree::slotVideoBrowser()
{
    jumpTo(JUMP_VIDEO_BROWSER);
}

void VideoTree::slotVideoGallery()
{
    jumpTo(JUMP_VIDEO_GALLERY);
}

void VideoTree::slotDoCancel(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();
}

void VideoTree::cancelPopup(void)
{
    //allowPaint = true;
    expectingPopup = false;

    if (popup)
    {
        popup->hide();
        delete popup;

        popup = NULL;

        updateForeground();
        qApp->processEvents();
        setActiveWindow();
    }
}

void VideoTree::slotViewPlot()
{
    cancelPopup();

    if (curitem)
    {
        //allowPaint = false;
        MythPopupBox * plotbox = new MythPopupBox(gContext->GetMainWindow());

        QLabel *plotLabel = plotbox->addLabel(curitem->Plot(),
                                              MythPopupBox::Small,true);
        plotLabel->setAlignment(Qt::AlignJustify | Qt::WordBreak);

        QButton * okButton = plotbox->addButton(tr("Ok"));
        okButton->setFocus();

        plotbox->ExecPopup();
        delete plotbox;
        //allowPaint = true;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("no Item to view"));
    }
}

void VideoTree::slotWatchVideo()
{
    cancelPopup();

    if (curitem)
        playVideo(curitem);
    else
        VERBOSE(VB_IMPORTANT, QString("no Item to watch"));
}

void VideoTree::playVideo(Metadata *someItem)
{
    if (!someItem)
        return;

    PlayVideo(someItem->ID(), m_video_list->getListCache());

    m_imp->video_tree_list->deactivate();
    gContext->GetMainWindow()->raise();
    gContext->GetMainWindow()->setActiveWindow();
    gContext->GetMainWindow()->currentWidget()->setFocus();

    updateForeground();
}

void VideoTree::slotDoFilter()
{
    cancelPopup();
    BasicFilterSettingsProxy<VideoList> sp(*m_video_list);
    VideoFilterDialog *vfd =
            new VideoFilterDialog(&sp, gContext->GetMainWindow(),
                                  "filter", "video-", *m_video_list,
                                  "Video Filter Dialog");
    vfd->exec();
    delete vfd;

    buildVideoList();
}

void VideoTree::jumpTo(const QString &location)
{
    cancelPopup();
    setExitType(SCREEN_EXIT_VIA_JUMP);
    gContext->GetMainWindow()->JumpTo(location);
}
