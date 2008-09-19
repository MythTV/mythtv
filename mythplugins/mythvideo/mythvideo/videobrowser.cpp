#include <unistd.h>
#include <algorithm>

#include <QPixmap>
#include <QPainter>
#include <QPaintEvent>
#include <QKeyEvent>

#include <mythtv/mythcontext.h>
#include <mythtv/xmlparse.h>
#include <mythtv/libmythui/mythuihelper.h>

#include "metadata.h"
#include "videobrowser.h"

#include "videofilter.h"
#include "videolist.h"
#include "videoutils.h"
#include "imagecache.h"
#include "parentalcontrols.h"

namespace
{
    int limit_upper(int value, int max)
    {
        return std::min(value, max);
    }
};

VideoBrowser::VideoBrowser(MythMainWindow *lparent, const QString &lname,
                           VideoList *video_list) :
    VideoDialog(DLG_BROWSER, lparent, "browser", lname, video_list),
    inData(0), m_state(0)
{
    setFlatList(true);

    setFileBrowser(gContext->GetNumSetting("VideoBrowserNoDB", 0));
    loadWindow(xmldata);
    bgTransBackup.reset(GetMythUI()->LoadScalePixmap("trans-backup.png"));

    if (!bgTransBackup.get())
        bgTransBackup.reset(new QPixmap());

    setNoErase();

    fetchVideos();
    updateBackground();
}

VideoBrowser::~VideoBrowser()
{
}

void VideoBrowser::slotParentalLevelChanged()
{
    LayerSet *container = theme->GetSet("browsing");
    if (container)
    {
        checkedSetText(container, "pl_value",
                       QString::number(currentParentalLevel->GetLevel()));
    }
}

void VideoBrowser::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList lactions;

    gContext->GetMainWindow()->TranslateKeyPress("Video", e, lactions);
    for (QStringList::const_iterator p = lactions.begin();
         p != lactions.end() && !handled; ++p)
    {
        QString action = *p;
        handled = true;

        if ((action == "SELECT" || action == "PLAY") && curitem)
            playVideo(curitem);
        else if (action == "INFO")
            doMenu(true);
        else if (action == "DOWN")
            jumpSelection(1);
        else if (action == "UP")
            jumpSelection(-1);
        else if (action == "PAGEDOWN")
            jumpSelection(limit_upper(m_video_list->count() / 5, 25));
        else if (action == "PAGEUP")
            jumpSelection(-limit_upper(m_video_list->count() / 5, 25));
        else if (action == "LEFT")
            cursorLeft();
        else if (action == "RIGHT")
            cursorRight();
        else if (action == "HOME")
            jumpToSelection(0);
        else if (action == "END")
            jumpToSelection(m_video_list->count() - 1);
        else if (action == "INCPARENT")
            shiftParental(1);
        else if (action == "DECPARENT")
            shiftParental(-1);
        else if (action == "1" || action == "2" ||
                action == "3" || action == "4")
            setParentalLevel(ParentalLevel(action.toInt()));
        else if (action == "FILTER")
            slotDoFilter();
        else if (action == "MENU")
            doMenu(false);
        else
            handled = false;
    }

    if (!handled)
    {
        gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e,
                                                     lactions);

        for (QStringList::const_iterator p = lactions.begin();
             p != lactions.end() && !handled; ++p)
        {
            if (*p == "PLAYBACK")
            {
                handled = true;
                slotWatchVideo();
            }
        }
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void VideoBrowser::doMenu(bool info)
{
    if (createPopup())
    {
        QAbstractButton *focusButton = NULL;
        if (info)
        {
            focusButton = popup->addButton(tr("Watch This Video"), this,
                                           SLOT(slotWatchVideo()));
            popup->addButton(tr("View Full Plot"), this, SLOT(slotViewPlot()));
            popup->addButton(tr("View Cast"), this, SLOT(slotViewCast()));
        }
        else
        {
            focusButton = popup->addButton(tr("Filter Display"), this,
                                           SLOT(slotDoFilter()));
            AddPopupViews();
        }

        popup->addButton(tr("Cancel"), this, SLOT(slotDoCancel()));

        popup->ShowPopup(this, SLOT(slotDoCancel()));

        if (focusButton)
            focusButton->setFocus();
    }
}

void VideoBrowser::fetchVideos()
{
    VideoDialog::fetchVideos();

    SetCurrentItem(0);
    update(infoRect);
    update(browsingRect);
    repaint();
}

void VideoBrowser::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);
    if (m_state == 0 && allowPaint)
    {
       if (r.intersects(infoRect))
       {
           updateInfo(&p);
       }

       if (r.intersects(browsingRect))
       {
           updateBrowsing(&p);
       }
    }
    else if (m_state > 0)
    {
        allowPaint = false;
        updatePlayWait(&p);
    }
}

void VideoBrowser::updatePlayWait(QPainter *p)
{
    if (m_state < 4)
    {
        LayerSet *container = theme->GetSet("playwait");
        if (container)
        {
            QRect area = container->GetAreaRect();
            if (area.isValid())
            {
                QPixmap pix(area.size());
                pix.fill(this, area.topLeft());
                QPainter tmp(&pix);

                for (int i = 0; i < 4; ++i)
                    container->Draw(&tmp, i, 0);

                tmp.end();

                p->drawPixmap(area.topLeft(), pix);
            }
            else
            {
                VERBOSE(VB_IMPORTANT,
                        QObject::tr("Theme Error: browser/playwait "
                                    "has an invalid area."));
            }
        }

        m_state++;
        update(fullRect);
    }
    else if (m_state == 4)
    {
        allowPaint = true;
        update(fullRect);
    }
}

void VideoBrowser::SetCurrentItem(unsigned int index)
{
    curitem = NULL;

    unsigned int list_count = m_video_list->count();

    if (list_count == 0)
        return;

    inData = QMIN(list_count - 1, index);
    curitem = m_video_list->getVideoListMetadata(inData);
}

void VideoBrowser::updateBrowsing(QPainter *p)
{
    QRect pr = browsingRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    unsigned int list_count = m_video_list->count();

    QString vidnum;
    if (list_count > 0)
        vidnum = QString(tr("%1 of %2")).arg(inData + 1).arg(list_count);
    else
        vidnum = tr("No Videos");

    LayerSet *container = theme->GetSet("browsing");
    if (container)
    {
        checkedSetText(container, "currentvideo", vidnum);

        checkedSetText(container, "pl_value",
                       QString::number(currentParentalLevel->GetLevel()));

        for (int i = 1; i < 9; ++i)
            container->Draw(&tmp, i, 0);
    }
    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}

void VideoBrowser::updateInfo(QPainter *p)
{
    if (m_video_list->count() > 0 && curitem)
    {
        LayerSet *container = theme->GetSet("info");
        if (container)
        {
            QRect pr = infoRect;
            QPixmap pix(pr.size());
            pix.fill(this, pr.topLeft());
            QPainter tmp(&pix);

            checkedSetText(container, "title", curitem->Title());
            checkedSetText(container, "filename", curitem->Filename());
            checkedSetText(container, "video_player",
                           Metadata::getPlayer(curitem));

            QString coverfile = curitem->CoverFile();
            UIImageType *itype = (UIImageType *)container->GetType("coverart");
            if (itype)
            {
                if (isDefaultCoverFile(coverfile))
                {
                    if (itype->isShown())
                        itype->hide();
                }
                else
                {
                    QSize img_size = itype->GetSize(true);
                    const QPixmap *img =
                            ImageCache::getImageCache().load(coverfile,
                                                                img_size.width(),
                                                                img_size.height(),
                                                                Qt::IgnoreAspectRatio);
                    if (img)
                    {
                        if (itype->GetImage().serialNumber() !=
                            img->serialNumber())
                        {
                            itype->SetImage(*img);
                        }
                        if (itype->isHidden())
                            itype->show();
                    }
                    else
                    {
                        if (itype->isShown())
                            itype->hide();
                    }
                }
            }

            checkedSetText(container, "director", curitem->Director());
            checkedSetText(container, "plot", curitem->Plot());
            checkedSetText(container, "cast", GetCast(*curitem));
            checkedSetText(container, "rating",
                           getDisplayRating(curitem->Rating()));
            checkedSetText(container, "inetref", curitem->InetRef());
            checkedSetText(container, "year", getDisplayYear(curitem->Year()));
            checkedSetText(container, "userrating",
                           getDisplayUserRating(curitem->UserRating()));
            checkedSetText(container, "length",
                           getDisplayLength(curitem->Length()));
            checkedSetText(container, "coverfile", coverfile);
            checkedSetText(container, "child_id",
                           QString::number(curitem->ChildID()));
            checkedSetText(container, "browseable",
                           getDisplayBrowse(curitem->Browse()));
            checkedSetText(container, "category", curitem->Category());
            checkedSetText(container, "level",
                           QString::number(curitem->ShowLevel()));

            for (int i = 1; i < 9; ++i)
                container->Draw(&tmp, i, 0);

                tmp.end();
                p->drawPixmap(pr.topLeft(), pix);
        }
    }
    else
    {
        LayerSet *norec = theme->GetSet("novideos_info");
        if (norec)
        {
            QRect pr = norec->GetAreaRect();
            if (pr.isValid())
            {
                QPixmap pix(pr.size());
                pix.fill(this, pr.topLeft());
                QPainter tmp(&pix);

                for (int i = 1; i <= 9; ++i)
                {
                    norec->Draw(&tmp, i, 0);
                }

                tmp.end();

                p->drawPixmap(pr.topLeft(), pix);
            }
            else
            {
                VERBOSE(VB_IMPORTANT,
                        QObject::tr("Theme Error: browser/novideos_info "
                                    "has an invalid area."));
            }
        }
    }
}

void VideoBrowser::jumpToSelection(int index)
{
    SetCurrentItem(index);
    update(infoRect);
    update(browsingRect);
}

void VideoBrowser::jumpSelection(int amount)
{
    unsigned int list_count = m_video_list->count();

    if (list_count == 0)
        return;

    int index;

    if (amount >= 0 || inData >= (unsigned int)-amount)
        index = (inData + amount) % list_count;
    else
        index = list_count + amount + inData;  // unsigned arithmetic

    jumpToSelection(index);
}

void VideoBrowser::cursorLeft()
{
    exitWin();
}

void VideoBrowser::cursorRight()
{
    doMenu();
}

void VideoBrowser::parseContainer(QDomElement &element)
{
    QRect area;
    QString container_name;
    int context;
    theme->parseContainer(element, container_name, context, area);

    container_name = container_name.lower();
    if (container_name == "info")
        infoRect = area;
    if (container_name == "browsing")
        browsingRect = area;
}
