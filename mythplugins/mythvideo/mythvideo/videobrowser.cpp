#include <qpixmap.h>
#include <unistd.h>

#include <mythtv/mythcontext.h>
#include <mythtv/xmlparse.h>

#include "metadata.h"
#include "videobrowser.h"

#include "videofilter.h"
#include "videolist.h"
#include "videoutils.h"
#include "imagecache.h"

VideoBrowser::VideoBrowser(MythMainWindow *lparent, const QString &lname,
                           VideoList *video_list) :
    VideoDialog(DLG_BROWSER, lparent, "browser", lname, video_list),
    inData(0), m_state(0)
{
    setFlatList(true);

    setFileBrowser(gContext->GetNumSetting("VideoBrowserNoDB", 0));
    loadWindow(xmldata);
    bgTransBackup.reset(gContext->LoadScalePixmap("trans-backup.png"));

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
        checkedSetText((UITextType *)container->GetType("pl_value"),
                       QString::number(currentParentalLevel));
    }
}

void VideoBrowser::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;

    gContext->GetMainWindow()->TranslateKeyPress("Video", e, actions);
    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
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
            jumpSelection(m_video_list->count() / 5);
        else if (action == "PAGEUP")
            jumpSelection(-(m_video_list->count() / 5));
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
            setParentalLevel(action.toInt());
        else if (action == "FILTER")
            slotDoFilter();
        else if (action == "MENU")
            doMenu(false);
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
        QButton *focusButton = NULL;
        if (info)
        {
            focusButton = popup->addButton(tr("Watch This Video"), this,
                                           SLOT(slotWatchVideo()));
            popup->addButton(tr("View Full Plot"), this, SLOT(slotViewPlot()));
        }
        else
        {
            if (!isFileBrowser)
                focusButton = popup->addButton(tr("Filter Display"), this,
                                               SLOT(slotDoFilter()));

            QButton* tempButton = addDests();
            if (!focusButton)
                focusButton = tempButton;
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

void VideoBrowser::grayOut(QPainter *tmp)
{
    tmp->fillRect(QRect(QPoint(0, 0), size()),
                  QBrush(QColor(10, 10, 10), Dense4Pattern));
}

void VideoBrowser::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);
    if (m_state == 0)
    {
       if (r.intersects(infoRect) && allowPaint == true)
       {
           updateInfo(&p);
       }
       if (r.intersects(browsingRect) && allowPaint == true)
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
        backup.flush();
        backup.begin(this);
        if (m_state == 1)
            grayOut(&backup);
        backup.end();

        LayerSet *container = NULL;
        container = theme->GetSet("playwait");
        if (container)
        {
            for (int i = 0; i < 4; ++i)
                container->Draw(p, i, 0);
        }
        m_state++;
        update(fullRect);
    }
    else if (m_state == 4)
    {
        backup.begin(this);
        backup.drawPixmap(0, 0, myBackground);
        backup.end();
        allowPaint = true;
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
        checkedSetText((UITextType *)container->GetType("currentvideo"),
                       vidnum);

        checkedSetText((UITextType *)container->GetType("pl_value"),
                       QString::number(currentParentalLevel));

        for (int i = 1; i < 9; ++i)
            container->Draw(&tmp, i, 0);
    }
    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}

void VideoBrowser::updateInfo(QPainter *p)
{
    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    if (m_video_list->count() > 0 && curitem)
    {
       LayerSet *container = theme->GetSet("info");
       if (container)
       {
           checkedSetText((UITextType *)container->GetType("title"),
                          curitem->Title());
           checkedSetText((UITextType *)container->GetType("filename"),
                          curitem->Filename());
           checkedSetText((UITextType *)container->GetType("video_player"),
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
                                                            QImage::ScaleFree);
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

           checkedSetText((UITextType *)container->GetType("director"),
                          curitem->Director());
           checkedSetText((UITextType *)container->GetType("plot"),
                          curitem->Plot());
           checkedSetText((UITextType *)container->GetType("rating"),
                          getDisplayRating(curitem->Rating()));
           checkedSetText((UITextType *)container->GetType("inetref"),
                          curitem->InetRef());
           checkedSetText((UITextType *)container->GetType("year"),
                          getDisplayYear(curitem->Year()));
           checkedSetText((UITextType *)container->GetType("userrating"),
                          getDisplayUserRating(curitem->UserRating()));
           checkedSetText((UITextType *)container->GetType("length"),
                          getDisplayLength(curitem->Length()));
           checkedSetText((UITextType *)container->GetType("coverfile"),
                          coverfile);
           checkedSetText((UITextType *)container->GetType("child_id"),
                          QString::number(curitem->ChildID()));
           checkedSetText((UITextType *)container->GetType("browseable"),
                          getDisplayBrowse(curitem->Browse()));
           checkedSetText((UITextType *)container->GetType("category"),
                          curitem->Category());
           checkedSetText((UITextType *)container->GetType("level"),
                          QString::number(curitem->ShowLevel()));

           for (int i = 1; i < 9; ++i)
               container->Draw(&tmp, i, 0);
       }
    }
    else
    {
       LayerSet *norec = theme->GetSet("novideos_info");
       if (norec)
       {
           for (int i = 4; i < 9; ++i)
               norec->Draw(&tmp, i, 0);
       }
    }
    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
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
