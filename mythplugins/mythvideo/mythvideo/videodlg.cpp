#include <cstdlib>

#include <QApplication>
#include <QPixmap>
#include <QLabel>
#include <QPainter>

#include <mythtv/xmlparse.h>
#include <mythtv/mythcontext.h>

#include "globals.h"
#include "videodlg.h"
#include "metadata.h"
#include "videofilter.h"
#include "videolist.h"
#include "videoutils.h"
#include "parentalcontrols.h"
#include "videoui-common.h"

bool VideoDialog::IsValidDialogType(int num)
{
    for (int i = 1; i <= dtLast - 1; i <<= 1)
        if (num == i) return true;
    return false;
}

VideoDialog::VideoDialog(DialogType ltype, MythMainWindow *lparent,
                         const QString &lwinName, const QString &lname,
                         VideoList *video_list) :
    MythDialog(lparent, lname), curitem(NULL),
    popup(NULL), m_type(ltype), m_video_list(video_list), m_exit_type(0)
{
    //
    //  Load the theme. Crap out if we can't find it.
    //

    theme.reset(new XMLParse());
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    if (!theme->LoadTheme(xmldata, lwinName, "video-"))
    {
        VERBOSE(VB_IMPORTANT,
                QString("VideoDialog: Couldn't find your theme. I'm outta here"
                        " (%1-video-ui)").arg(lwinName));
        exit(0);
    }

    expectingPopup = false;
    fullRect = QRect(0, 0, size().width(), size().height());
    allowPaint = true;
    currentParentalLevel.reset(new ParentalLevel(
                gContext->GetNumSetting("VideoDefaultParentalLevel", 1)));

    // Do no allow a default parental level to bypass the password
    // check.
    if (!checkParentPassword(currentParentalLevel->GetLevel()))
    {
        *currentParentalLevel = ParentalLevel::plLowest;
    }

    VideoFilterSettings video_filter(true, lwinName);
    m_video_list->setCurrentVideoFilter(video_filter);

    isFileBrowser = false;
    isFlatList = false;
    video_tree_root = NULL;
}

VideoDialog::~VideoDialog()
{
}

void VideoDialog::slotVideoBrowser()
{
    jumpTo(JUMP_VIDEO_BROWSER);
}

void VideoDialog::slotVideoGallery()
{
    jumpTo(JUMP_VIDEO_GALLERY);
}

void VideoDialog::slotVideoTree()
{
    jumpTo(JUMP_VIDEO_TREE);
}

void VideoDialog::slotDoCancel(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();
}

void VideoDialog::cancelPopup(void)
{
    allowPaint = true;
    expectingPopup = false;

    if (popup)
    {
        popup->deleteLater();
        popup = NULL;

        update(fullRect);
        qApp->processEvents();
        setActiveWindow();
    }
}

QAbstractButton *VideoDialog::AddPopupViews()
{
    if (!popup)
        return NULL;

    std::vector<QAbstractButton *> buttons;

    if (!(m_type & DLG_BROWSER))
        buttons.push_back(popup->addButton(tr("Switch to Browse View"), this,
                                           SLOT(slotVideoBrowser())));

    if (!(m_type & DLG_GALLERY))
        buttons.push_back(popup->addButton(tr("Switch to Gallery View"), this,
                                           SLOT(slotVideoGallery())));

    if (!(m_type & DLG_TREE))
        buttons.push_back(popup->addButton(tr("Switch to List View"), this,
                                 SLOT(slotVideoTree())));

    return buttons.size() ? buttons[0] : NULL;
}

bool VideoDialog::createPopup()
{
    if (!popup)
    {
        allowPaint = false;
        popup = new MythPopupBox(gContext->GetMainWindow(), "video popup");

        expectingPopup = true;

        popup->addLabel(tr("Select action"));
        popup->addLabel("");
    }

    return (popup != NULL);
}


void VideoDialog::slotViewPlot()
{
    cancelPopup();

    if (curitem)
    {
        allowPaint = false;
        MythPopupBox * plotbox = new MythPopupBox(gContext->GetMainWindow());

        QLabel *plotLabel = plotbox->addLabel(curitem->Plot(),
                                              MythPopupBox::Small,true);
        plotLabel->setAlignment(Qt::AlignJustify | Qt::WordBreak);

        QAbstractButton *okButton = plotbox->addButton(
            tr("OK"), plotbox, SLOT(accept()));
        okButton->setFocus();

        plotbox->ExecPopup();
        plotbox->deleteLater();
        allowPaint = true;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("no Item to view"));
    }
}

void VideoDialog::slotViewCast()
{
    cancelPopup();

    if (curitem)
    {
        allowPaint = false;
        ShowCastDialog(gContext->GetMainWindow(), *curitem);
        allowPaint = true;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("no item to view"));
    }
}

void VideoDialog::slotWatchVideo()
{
    cancelPopup();

    if (curitem)
        playVideo(curitem);
    else
        VERBOSE(VB_IMPORTANT, QString("no Item to watch"));
}

void VideoDialog::playVideo(Metadata *someItem)
{
    // Show a please wait message
    LayerSet *container = theme->GetSet("playwait");
    if (container)
    {
        checkedSetText(container, "title", someItem->Title());
    }
    update(fullRect);
    allowPaint = false;

    PlayVideo(someItem->Filename(), m_video_list->getListCache());

    gContext->GetMainWindow()->raise();
    gContext->GetMainWindow()->setActiveWindow();
    if (gContext->GetMainWindow()->currentWidget())
        gContext->GetMainWindow()->currentWidget()->setFocus();

    allowPaint = true;

    update(fullRect);
}

void VideoDialog::setParentalLevel(const ParentalLevel &which_level)
{
    ParentalLevel::Level new_level = which_level.GetLevel();
    if (new_level == ParentalLevel::plNone)
        new_level = ParentalLevel::plLowest;

    if (checkParentPassword(new_level, currentParentalLevel->GetLevel()) &&
                            *currentParentalLevel != new_level)
    {
        *currentParentalLevel = new_level;
        fetchVideos();
        slotParentalLevelChanged();
    }
}

void VideoDialog::shiftParental(int amount)
{
    setParentalLevel(ParentalLevel(*currentParentalLevel) += amount);
}

GenericTree *VideoDialog::getVideoTreeRoot(void)
{
    return video_tree_root;
}

void VideoDialog::fetchVideos()
{
    video_tree_root = m_video_list->buildVideoList(isFileBrowser, isFlatList,
                                                   *currentParentalLevel, true);
}

void VideoDialog::slotDoFilter()
{
    cancelPopup();
    BasicFilterSettingsProxy<VideoList> sp(*m_video_list);
    VideoFilterDialog *vfd = new VideoFilterDialog(&sp,
                                                    gContext->GetMainWindow(),
                                                    "filter", "video-",
                                                    *m_video_list,
                                                    "Video Filter Dialog");
    vfd->exec();
    delete vfd;

    fetchVideos();
}

void VideoDialog::exitWin()
{
    emit accept();
}

void VideoDialog::slotParentalLevelChanged()
{
}

void VideoDialog::loadWindow(QDomElement &element)
{
    for (QDomNode lchild = element.firstChild(); !lchild.isNull();
         lchild = lchild.nextSibling())
    {
        QDomElement e = lchild.toElement();

        if (!e.isNull())
        {
            if (e.tagName() == "font")
            {
                theme->parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                this->parseContainer(e);
            }
            else
            {
                MythPopupBox::showOkPopup(gContext->GetMainWindow(), "",
                    tr(QString("There is a problem with your video-ui.xml "
                               "file... Unknown element: %1").
                       arg(e.tagName())));

                VERBOSE(VB_IMPORTANT, QString("Unknown element: %1")
                        .arg(e.tagName()));
            }
        }
    }
}

void VideoDialog::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
    if (container)
        container->Draw(&tmp, 0, 0);

    tmp.end();
    myBackground = bground;

    setPaletteBackgroundPixmap(myBackground);
}

void VideoDialog::jumpTo(const QString &location)
{
    cancelPopup();
    setExitType(SCREEN_EXIT_VIA_JUMP);
    gContext->GetMainWindow()->JumpTo(location);
}
