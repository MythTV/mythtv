#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qaccel.h>
#include <qlistview.h>
#include <qdatetime.h>
#include <qprogressbar.h>
#include <qlayout.h>
#include <qapplication.h>
#include <qtimer.h>
#include <qimage.h>
#include <qpainter.h>
#include <qheader.h>
#include <qfile.h>
#include <qsqldatabase.h>

#include <unistd.h>

#include <iostream>
using namespace std;

#include "playbackbox.h"
#include "tv.h"
#include "oldsettings.h"
#include "NuppelVideoPlayer.h"
#include "yuv2rgb.h"

#include "mythcontext.h"
#include "programinfo.h"
#include "remoteutil.h"

PlaybackBox::PlaybackBox(BoxType ltype, MythMainWindow *parent, 
                         const char *name)
           : MythDialog(parent, name)
{
    type = ltype;

    rbuffer = NULL;
    nvp = NULL;

    ignoreevents = false;

    titleitems = 0;         // Number of items available in list, = showData.count() for "All Programs"
                         // In other words, this is the number of shows selected by the title selector
    listCount = 0;         // How full the actual list is (list in this context being the number
                         // of spaces available for data to be shown).
    inTitle = true;      // Cursor in Title Listing
    skipNum = 0;         // Amount of records to skip (for scrolling)
    curShowing = 0;         // Where in the list (0 - # in list)
    pageDowner = false;         // Is there enough records to page down?
    skipUpdate = false;
    skipCnt = 0;

    leftRight = false;   // If change is left or right, don't restart video
    playingVideo = false;
    graphicPopup = true;

    titleData = NULL;
    popup = NULL;
    curitem = NULL;
    delitem = NULL;

    fullRect = QRect(0, 0, size().width(), size().height());
    listRect = QRect(0, 0, 0, 0);
    infoRect = QRect(0, 0, 0, 0);
    usageRect = QRect(0, 0, 0, 0);
    videoRect = QRect(0, 0, 0, 0);

    connect(this, SIGNAL(killTheApp()), this, SLOT(accept()));

    noUpdate = false;

    showDateFormat = gContext->GetSetting("ShortDateFormat", "M/d");
    showTimeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");

    bgTransBackup = gContext->LoadScalePixmap("trans-backup.png");
    if (!bgTransBackup)
        bgTransBackup = new QPixmap();

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    theme->LoadTheme(xmldata, "playback");

    LoadWindow(xmldata);

    LayerSet *container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("showing");
        if (ltype)
            listsize = ltype->GetItems();
    }
    else
    {
        cerr << "Failed to get selector object.\n";
        exit(0);
    }

    FillList(); 

    curTitle = 0;

    playbackPreview = gContext->GetNumSetting("PlaybackPreview");
    generatePreviewPixmap = gContext->GetNumSetting("GeneratePreviewPixmaps");
    displayChanNum = gContext->GetNumSetting("DisplayChanNum");
    dateformat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");

    nvp = NULL;
    timer = new QTimer(this);

    updateBackground();

    setNoErase();

    connect(timer, SIGNAL(timeout()), this, SLOT(timeout()));

    timer->start(1000 / 30);
    gContext->addListener(this);
}

PlaybackBox::~PlaybackBox(void)
{
    gContext->removeListener(this);
    killPlayer();
    delete timer;
    delete theme;
    delete bgTransBackup;
    if (curitem)
        delete curitem;
    if (titleData)
        delete [] titleData;
}

void PlaybackBox::LoadWindow(QDomElement &element)
{
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
            {
                theme->parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                parseContainer(e);
            }
            else if (e.tagName() == "popup")
            {
                parsePopup(e);
            }
            else
            {
                cerr << "Unknown element: " << e.tagName() << endl;
                exit(0);
            }
        }
    }
}

void PlaybackBox::parseContainer(QDomElement &element)
{
    QRect area;
    QString name;
    int context; 
    theme->parseContainer(element, name, context, area);

    if (name.lower() == "selector")
        listRect = area;
    if (name.lower() == "program_info_play" && context == 0 && type != Delete)
        infoRect = area;
    if (name.lower() == "program_info_del" && context == 1 && type == Delete)
        infoRect = area;
    if (name.lower() == "video")
        videoRect = area;
    if (name.lower() == "usage")
        usageRect = area;
}

void PlaybackBox::parsePopup(QDomElement &element)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Popup needs a name\n";
        exit(0);
    }

    if (name != "confirmdelete")
    {
        cerr << "Unknown popup name! (try using 'confirmdelete')\n";
        exit(0);
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "solidbgcolor")
            {
                QString col = theme->getFirstText(info);
                popupBackground = QColor(col);
                graphicPopup = false;
            }
            else if (info.tagName() == "foreground")
            {
                QString col = theme->getFirstText(info);
                popupForeground = QColor(col);
            }
            else if (info.tagName() == "highlight")
            {
                QString col = theme->getFirstText(info);
                popupHighlight = QColor(col);
            }
            else
            {
                cerr << "Unknown popup child: " << info.tagName() << endl;
                exit(0);
            }
        }
    }
}

void PlaybackBox::exitWin()
{
    if (ignoreevents)
        return;

    noUpdate = true;
    timer->stop();
    killPlayer();

    emit killTheApp();
}

void PlaybackBox::updateBackground(void)
{
    if (noUpdate == true)
        return; 

    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
    if (container && type != Delete)
        container->Draw(&tmp, 0, 0);
    else
        container->Draw(&tmp, 0, 1);

    tmp.end();
    myBackground = bground;
 
    setPaletteBackgroundPixmap(myBackground);
}
  
void PlaybackBox::paintEvent(QPaintEvent *e)
{
    if (e->erased())
        skipUpdate = false;

    if (noUpdate == false)
    {
        QRect r = e->rect();
        QPainter p(this);

        if (r.intersects(listRect) && skipUpdate == false)
        {
            updateShowTitles(&p);
        }
        if (r.intersects(infoRect) && skipUpdate == false)
        {
            updateInfo(&p);
        }
        if (r.intersects(usageRect) && skipUpdate == false)
        {
            updateUsage(&p);
        }
        if (r.intersects(videoRect) && ignoreevents == false)
        {
            updateVideo(&p);
        }
        skipCnt--;
        if (skipCnt < 0)
        {
            skipUpdate = true;
            skipCnt = 0;
        }
    }
}

void PlaybackBox::grayOut(QPainter *tmp)
{
    if (noUpdate == false)
    {
        int transparentFlag = gContext->GetNumSetting("PlayBoxShading", 0);
        if (transparentFlag == 0)
            tmp->fillRect(QRect(QPoint(0, 0), size()), 
                          QBrush(QColor(10, 10, 10), Dense4Pattern));
        else if (transparentFlag == 1)
            tmp->drawPixmap(0, 0, *bgTransBackup, 0, 0, (int)(800*wmult), 
                            (int)(600*hmult));
    }
}

void PlaybackBox::updateInfo(QPainter *p)
{
    if (noUpdate == true)
        return;

    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    if (showData.count() > 0 && curitem)
    {
        QSqlDatabase *m_db = QSqlDatabase::database();
        ignoreevents = true;

        if (playingVideo == true)
            killPlayer();

        QDateTime startts = curitem->startts;
        QDateTime endts = curitem->endts;

        QString timedate = startts.date().toString(dateformat) + ", " +
                           startts.time().toString(timeformat) + " - " +
                           endts.time().toString(timeformat);

        QString chantext = "";
        if (displayChanNum)
            chantext = curitem->channame + " [" + curitem->chansign + "]";
        else
            chantext = curitem->chanstr;

        QString subtitle = curitem->subtitle;
        if (subtitle.length() > 1)
            subtitle = "\"" + curitem->subtitle + "\"";

        LayerSet *container = NULL;
        if (type != Delete)
            container = theme->GetSet("program_info_play");
        else
            container = theme->GetSet("program_info_del");
        if (container)
        {
            UITextType *type = (UITextType *)container->GetType("title");
            if (type)
                type->SetText(curitem->title);

            type = (UITextType *)container->GetType("subtitle");
            if (type)
                type->SetText(subtitle);
    
            type = (UITextType *)container->GetType("timedate");
            if (type)
                type->SetText(timedate);

            type = (UITextType *)container->GetType("description");
            if (type)
                type->SetText(curitem->description);

            type = (UITextType *)container->GetType("channel");
            if (type)
                type->SetText(chantext);

            UIImageType *itype;
            itype = (UIImageType *)container->GetType("commflagged");
            if (itype)
            {
                QMap<long long, int> commbreaks;
                curitem->GetCommBreakList(commbreaks, m_db);
                if (commbreaks.size())
                    itype->ResetFilename();
                else
                    itype->SetImage("blank.png");
                itype->LoadImage();
            }

            itype = (UIImageType *)container->GetType("cutlist");
            if (itype)
            {
                QMap<long long, int> cutlist;
                curitem->GetCutList(cutlist, m_db);
                if (cutlist.size())
                    itype->ResetFilename();
                else
                    itype->SetImage("blank.png");
                itype->LoadImage();
            }

            itype = (UIImageType *)container->GetType("autoexpire");
            if (itype)
            {
                if (curitem->GetAutoExpireFromRecorded(m_db))
                    itype->ResetFilename();
                else
                    itype->SetImage("blank.png");
                itype->LoadImage();
            }

            itype = (UIImageType *)container->GetType("processing");
            if (itype)
            {
                if ((curitem->IsEditing(m_db)) ||
                    (curitem->CheckMarkupFlag(MARK_PROCESSING, m_db)))
                    itype->ResetFilename();
                else
                    itype->SetImage("blank.png");
                itype->LoadImage();
            }

            itype = (UIImageType *)container->GetType("bookmark");
            if (itype)
            {
                if (curitem->GetBookmark(m_db))
                    itype->ResetFilename();
                else
                    itype->SetImage("blank.png");
                itype->LoadImage();
            }
        }

        if (container && type != Delete)
            container->Draw(&tmp, 6, 0);
        else
            container->Draw(&tmp, 6, 1);

        tmp.end();
        p->drawPixmap(pr.topLeft(), pix);

        ignoreevents = false;

        timer->start(1000 / 30);
    }
    else
    {
        LayerSet *norec = theme->GetSet("norecordings_info");
        if (type != Delete && norec)
            norec->Draw(&tmp, 8, 0);
        else if (norec)
            norec->Draw(&tmp, 8, 1);

        tmp.end();
        p->drawPixmap(pr.topLeft(), pix);
    }
}

void PlaybackBox::updateVideo(QPainter *p)
{
    if (noUpdate == true)
        return;

    if (playbackPreview == 0 && curitem)
    {
        QPixmap temp = getPixmap(curitem);
        if (temp.width() > 0)
        {
            p->drawPixmap(videoRect.x(), videoRect.y(), temp);
        }
    }
    else
    {
        if (!nvp)
            return;

        int w = 0, h = 0;
        unsigned char *buf = nvp->GetCurrentFrame(w, h);

        if (w == 0 || h == 0 || !buf)
            return;

        unsigned char *outputbuf = new unsigned char[w * h * 4];
        yuv2rgb_fun convert = yuv2rgb_init_mmx(32, MODE_RGB);

        convert(outputbuf, buf, buf + (w * h), buf + (w * h * 5 / 4), w, h, 
                w * 4, w, w / 2);

        QImage img(outputbuf, w, h, 32, NULL, 65536 * 65536, 
                   QImage::LittleEndian);
        img = img.scale(videoRect.width(), videoRect.height());

        delete [] outputbuf;

        p->drawImage(videoRect.x(), videoRect.y(), img);
    }
}

void PlaybackBox::updateUsage(QPainter *p)
{
    if (noUpdate == true)
        return; 

    int total, used;
    noUpdate = true;
    RemoteGetFreeSpace(total, used);
    noUpdate = false;

    QString usestr;

    double perc = (double)((double)used / (double)total);
    perc = ((double)100 * (double)perc);
    usestr.sprintf("%d", (int)perc);
    usestr = usestr + tr("% used");

    QString rep;
    rep.sprintf(tr(", %d,%03d MB free"), (total - used) / 1000, 
                                         (total - used) % 1000);
    usestr = usestr + rep;

    LayerSet *container = NULL;
    container = theme->GetSet("usage");
    if (container)
    {
        int ccontext = container->GetContext();
        
        if (ccontext != -1)
        {
            if (ccontext == 1 && type != Delete)
                return;
            if (ccontext == 0 && type == Delete)
                return;
        }

        QRect pr = usageRect;
        QPixmap pix(pr.size());
        pix.fill(this, pr.topLeft());
        QPainter tmp(&pix);

        UITextType *ttype = (UITextType *)container->GetType("freereport");
        if (ttype)
            ttype->SetText(usestr);

        UIStatusBarType *sbtype = (UIStatusBarType *)container->GetType("usedbar");
        if (sbtype)
        {
            sbtype->SetUsed(used);
            sbtype->SetTotal(total);
        }

        if (type != Delete)
        {
            container->Draw(&tmp, 5, 0);
            container->Draw(&tmp, 6, 0);
        }
        else
        {
            container->Draw(&tmp, 5, 1);
            container->Draw(&tmp, 6, 1);
        }

        tmp.end();
        p->drawPixmap(pr.topLeft(), pix);
    }
}

void PlaybackBox::updateShowTitles(QPainter *p)
{
    if (noUpdate == true)
        return; 

    int cnt = 0;
    int h = 0;
    int pastSkip = (int)skipNum;
    pageDowner = false;

    QString tempTitle;
    QString tempSubTitle;
    QString tempDate;
    QString tempTime;
    QString tempSize;
    bool tempCurrent;

    QString match;
    QRect pr = listRect;
    QPixmap pix(pr.size());

    LayerSet *container = NULL;
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    container = theme->GetSet("selector");

    typedef QMap<QString, ProgramInfo> ShowData;
    ProgramInfo *tempInfo;

    QDateTime curtime = QDateTime::currentDateTime();

    ShowData::Iterator it;
    ShowData::Iterator start;
    ShowData::Iterator end;
    if (titleData[curTitle] != tr("All Programs"))
    {
        start = showData.begin();
        end = showData.end();
    }
    else
    {
        start = showDateData.begin();
        end = showDateData.end();
    }

    if (container)
    {
        int itemCnt = 0;
        UIListType *ltype = (UIListType *)container->GetType("toptitles");
        if (ltype)
        {
            int cnt = 0;
            ltype->ResetList();
            ltype->SetActive(inTitle);

            itemCnt = ltype->GetItems();
            for (int i = curTitle - itemCnt; i < curTitle; i++)
            {
                h = i;
                if (i < 0)
                    h = i + showList.count();
                if (i > (signed int)(showList.count() - 1))
                    h = i - showList.count();

                ltype->SetItemText(cnt, titleData[h]);
                cnt++;
             }
        }

        ltype = (UIListType *)container->GetType("bottomtitles");
        if (ltype)
        {
            int cnt = 0;
            ltype->ResetList();
            ltype->SetActive(inTitle);

            itemCnt = ltype->GetItems();

            for (int i = curTitle + 1; i <= (curTitle + itemCnt); i++)
            {
                h = i;
                if (i < 0)
                    h = i + showList.count();
                if (i > (signed int)(showList.count() - 1))
                    h = i - showList.count();

                ltype->SetItemText(cnt, titleData[h]);
                cnt++;
            }
        }

        UITextType *typeText = (UITextType *)container->GetType("current");
        if (typeText)
            typeText->SetText(titleData[curTitle]);

        ltype = (UIListType *)container->GetType("showing");
        if (ltype)
        {
          ltype->ResetList();
          ltype->SetActive(!inTitle);
  
          titleitems = 0;
          for (it = start; it != end; ++it)
          {
             if (cnt < listsize)
             {
                 match = (it.key()).left((it.key()).find("-!-"));
                 if (match == (titleData[curTitle]).lower() 
                     || titleData[curTitle] == tr("All Programs"))
                 {
                     if (pastSkip <= 0)
                     {
                         tempInfo = &(it.data());

                         if (curtime < tempInfo->endts)
                             tempCurrent = true;
                         else
                             tempCurrent = false;

                         if (titleData[curTitle] == tr("All Programs"))
                             tempSubTitle = tempInfo->title; 
                         else
                              tempSubTitle = tempInfo->subtitle;
                           if (tempSubTitle.stripWhiteSpace().length() == 0)
                             tempSubTitle = tempInfo->title;
                         if ((tempInfo->subtitle).stripWhiteSpace().length() > 0 
                             && titleData[curTitle] == tr("All Programs"))
                         {
                             tempSubTitle = tempSubTitle + " - \"" + 
                                            tempInfo->subtitle + "\"";
                         }

                         tempDate = (tempInfo->startts).toString(showDateFormat);
                         tempTime = (tempInfo->startts).toString(showTimeFormat);

                         long long size = tempInfo->filesize;
                         long int mbytes = size / 1024 / 1024;
                         tempSize = QString("%1 MB").arg(mbytes);

                         if (cnt == curShowing)
                         {
                             if (curitem)
                                 delete curitem;
                             curitem = new ProgramInfo(*tempInfo);
                             ltype->SetItemCurrent(cnt);
                         }

                         ltype->SetItemText(cnt, 1, tempSubTitle);
                         ltype->SetItemText(cnt, 2, tempDate);
                         ltype->SetItemText(cnt, 3, tempTime);
                         ltype->SetItemText(cnt, 4, tempSize);
                         if (tempCurrent == true)
                             ltype->EnableForcedFont(cnt, "recording");

                            cnt++;
                     }
                     pastSkip--;
                     titleitems++;
                     pageDowner = false;

                 }  // match else
                 //else
                 //    pageDowner = true;
            } // cnt < listsiz else
            else
            {
                match = (it.key()).left( (it.key()).find("-!-") );
                if (match == (titleData[curTitle]).lower() || 
                    titleData[curTitle] == tr("All Programs"))
                {
                    titleitems++;
                    pageDowner = true;
                }
            } 
            // end of cnt < listsize if
         }
         // for (iterator)
       } 
       // end of type check
 
       ltype->SetDownArrow(pageDowner);
       if (skipNum > 0)
           ltype->SetUpArrow(true);
       else
           ltype->SetUpArrow(false);

    } 
    // end of container check

    listCount = cnt;

    // DRAW LAYERS
    if (container && type != Delete)
    {
        container->Draw(&tmp, 0, 0);
        container->Draw(&tmp, 1, 0);
        container->Draw(&tmp, 2, 0);
        container->Draw(&tmp, 3, 0);
        container->Draw(&tmp, 4, 0);
        container->Draw(&tmp, 5, 0);
        container->Draw(&tmp, 6, 0);
        container->Draw(&tmp, 7, 0);
        container->Draw(&tmp, 8, 0);
    }
    else
    {
        container->Draw(&tmp, 0, 1);
        container->Draw(&tmp, 1, 1);
        container->Draw(&tmp, 2, 1);
        container->Draw(&tmp, 3, 1);
        container->Draw(&tmp, 4, 1);
        container->Draw(&tmp, 5, 1);
        container->Draw(&tmp, 6, 1);
        container->Draw(&tmp, 7, 1);
        container->Draw(&tmp, 8, 1);
    }

    leftRight = false;

    if (showData.count() == 0)
    {
        LayerSet *norec = theme->GetSet("norecordings_list");
        if (type != Delete && norec)
            norec->Draw(&tmp, 8, 0);
        else if (norec)
            norec->Draw(&tmp, 8, 1);
    }
  
    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);

    if (showData.count() == 0)
        update(infoRect);
}

QString PlaybackBox::cutDown(QString info, QFont *testFont, int maxwidth)
{
    QFontMetrics fm(*testFont);

    int curFontWidth = fm.width(info);
    if (curFontWidth > maxwidth)
    {
        QString testInfo = "";
        curFontWidth = fm.width(testInfo);
        int tmaxwidth = maxwidth - fm.width("LLL");
        int count = 0;

        while (curFontWidth < tmaxwidth)
        {
            testInfo = info.left(count);
            curFontWidth = fm.width(testInfo);
            count = count + 1;
        }
        testInfo = testInfo + "...";
        info = testInfo;
    }
    return info;
}

void PlaybackBox::cursorLeft()
{
    inTitle = true;
    skipUpdate = false;
    update(fullRect);
    leftRight = true;
}

void PlaybackBox::cursorRight()
{
    leftRight = true;
    inTitle = false;
    skipUpdate = false;
    update(fullRect);
}

void PlaybackBox::cursorDown(bool page)
{
    if (inTitle == true)
    {
        skipNum = 0;
        curShowing = 0;
        if (page == false)
            curTitle++;
        else if (page == true)
            curTitle = curTitle + 5;

        if (curTitle >= (signed int)showList.count() && page == true)
            curTitle = curTitle - (signed int)showList.count();
        else if (curTitle >= (signed int)showList.count() && page == false)
            curTitle = 0;

        if (curTitle < 0)
            curTitle = 0;

        while (titleData[curTitle] == "***FILLER***")
        {
            curTitle++;
            if (curTitle >= (signed int)showList.count())
                curTitle = 0;
        }

        skipUpdate = false;
        update(fullRect);
    }
    else
    {
        if (page == false)
        {
            if (curShowing > (int)((int)(listsize / 2) - 1) 
                && ((int)(skipNum + listsize) <= (int)(titleitems - 1)) 
                && pageDowner == true)
            {
                skipNum++;
                curShowing = (int)(listsize / 2);
            }
            else
            {
                curShowing++;

                if (curShowing >= listCount)
                    curShowing = listCount - 1;
            }

        }
        else if (page == true && pageDowner == true)
        {
            if (curShowing >= (int)(listsize / 2) || skipNum != 0)
            {
                skipNum = skipNum + listsize;
            }
            else if (curShowing < (int)(listsize / 2) && skipNum == 0)
            {
                skipNum = (int)(listsize / 2) + curShowing;
                curShowing = (int)(listsize / 2);
            }
        }
        else if (page == true && pageDowner == false)
        {
            curShowing = listsize - 1;
        }

        if ((int)(skipNum + curShowing) >= (int)(titleitems - 1))
        {
            skipNum = titleitems - listsize;
            curShowing = listsize - 1;
        }
        else if ((int)(skipNum + listsize) >= (int)titleitems)
        {
            skipNum = titleitems - listsize;
        }

        if (curShowing >= listCount)
            curShowing = listCount - 1;

        skipUpdate = false;
        update(fullRect);
    }
}

void PlaybackBox::cursorUp(bool page)
{
    if (inTitle == true)
    {
        curShowing = 0;
        skipNum = 0;

        if (page == false)
            curTitle--;
          else
            curTitle = curTitle - 5;

        if (curTitle < 0 && page == false)
            curTitle = (signed int)showList.count() - 1;
        if (curTitle < 0 && page == true)
            curTitle = (signed int)showList.count() + curTitle;

        while (titleData[curTitle] == "***FILLER***")
        {
           curTitle--;
           if (curTitle < 0)
               curTitle = showList.count() - 1;
        }
        skipUpdate = false;
        update(fullRect);
    }
    else
    {
        if (page == false)
        {
            if (curShowing < ((int)(listsize / 2) + 1) && skipNum > 0)
            {
                     curShowing = (int)(listsize / 2);
                skipNum--;
                if (skipNum < 0)
                {
                     skipNum = 0;
                     curShowing--;
                }
            }
            else
            {
                curShowing--;
            }
        }
        else if (page == true && skipNum > 0)
        {
            skipNum = skipNum - listsize;
            if (skipNum < 0)
            {
                curShowing = curShowing + skipNum;
                skipNum = 0;
                if (curShowing < 0)
                    curShowing = 0;
            }

            if (curShowing > (int)(listsize / 2))        
            {
                curShowing = (int)(listsize / 2);
                skipNum = skipNum + (int)(listsize / 2) - 1;
            }
        }
        else if (page == true)
        {
            skipNum = 0;
            curShowing = 0;
        }

        if (curShowing > -1)
        {
            skipUpdate = false;
            update(fullRect);
        }
        else
            curShowing = 0;
    }
}

void PlaybackBox::FillList()
{
    QString chanid = "";
    typedef QMap<QString,QString> ShowData;
    int order = 1;
    order = gContext->GetNumSetting("PlayBoxOrdering", 1);
    int cnt = 999;
    if (order == 0 && type != Delete)
        cnt = 100;

    noUpdate = true;

    showData.clear();
    showDateData.clear();
    if (showList.count() > 0)
    {
        showList.clear();
    }

    if (titleData)
        delete [] titleData;
    titleData = NULL;

    showList[""] = tr("All Programs");

    vector<ProgramInfo *> *infoList;
    infoList = RemoteGetRecordedList(type == Delete);
    if (infoList)
    {
        QString temp;

        vector<ProgramInfo *>::iterator i = infoList->begin();
        for (; i != infoList->end(); i++)
        {
            showList[((*i)->title).lower()] = (*i)->title;
            temp = QString("%1-!-%2").arg(((*i)->title).lower()).arg(cnt);
            showData[temp] = *(*i);
            temp = QString("%1").arg(cnt);
            showDateData[temp] = *(*i);
            if (order == 0 && type != Delete)
                cnt++;
            else 
                cnt--;
            delete (*i);
        }
        delete infoList;
    }

    if ((signed int)showList.count() < (listsize - 1))
    {
        QString temp;
        titleData = new QString[listsize - 1];
        for (int j = showList.count(); j < (listsize - 1); j++)
        {
            temp = QString("%1").arg(j);
            showList[temp] = "***FILLER***";
            titleData[j] = "***FILLER***";
        }
    }
    else
        titleData = new QString[showList.count() + 1];

    cnt = 0;

    ShowData::Iterator it;
    for (it = showList.begin(); it != showList.end(); ++it)
    {
        if (it.data() != "***FILLER***")
        {
            titleData[cnt] = it.data();
            cnt++;
        }
    }

    lastUpdateTime = QDateTime::currentDateTime();

    noUpdate = false;
}

static void *SpawnDecoder(void *param)
{
    NuppelVideoPlayer *nvp = (NuppelVideoPlayer *)param;
    nvp->StartPlaying();
    return NULL;
}

void PlaybackBox::killPlayer(void)
{
    timer->stop();
    while (timer->isActive())
        usleep(50);

    playingVideo = false;

    if (nvp)
    {
        QDateTime curtime = QDateTime::currentDateTime();
        curtime = curtime.addSecs(2);

        ignoreevents = true;
        while (!nvp->IsPlaying())
        {
            if (QDateTime::currentDateTime() > curtime)
                break;
            usleep(50);
            qApp->unlock();
            qApp->processEvents();
            qApp->lock();
        }
        curtime = QDateTime::currentDateTime();
        curtime = curtime.addSecs(2);

        rbuffer->Pause();
        while (!rbuffer->isPaused())
        {
            usleep(50);
            qApp->unlock();
            qApp->processEvents();
            qApp->lock();
        }

        nvp->StopPlaying();

        while (nvp->IsPlaying())
        {
            if (QDateTime::currentDateTime() > curtime)
                break;
            usleep(50);
            qApp->unlock();
            qApp->processEvents();
            qApp->lock();
        }

        qApp->unlock();
        pthread_join(decoder, NULL);
        delete nvp;
        delete rbuffer;
        qApp->lock();

        nvp = NULL;
        rbuffer = NULL;

        ignoreevents = false;
    }
}        

void PlaybackBox::startPlayer(ProgramInfo *rec)
{
    playingVideo = true;

    if (rec != NULL)
    {
        if (rbuffer || nvp)
        {
            cerr << "ERROR: preview window didn't clean up\n";
            return;
        }

        rbuffer = new RingBuffer(rec->pathname, false, true);

        nvp = new NuppelVideoPlayer();
        nvp->SetRingBuffer(rbuffer);
        nvp->SetAsPIP();
        nvp->SetOSDFontName(gContext->GetSetting("OSDFont"),
                            gContext->GetSetting("OSDCCFont"),
                            gContext->GetInstallPrefix());
        QString filters = "";
        nvp->SetVideoFilters(filters);
 
        pthread_create(&decoder, NULL, SpawnDecoder, nvp);

        QDateTime curtime = QDateTime::currentDateTime();
        curtime = curtime.addSecs(2);
 
        ignoreevents = true;

        while (nvp && !nvp->IsPlaying())
        {
            if (QDateTime::currentDateTime() > curtime)
                break;
            usleep(50);
            qApp->unlock();
            qApp->processEvents();
            qApp->lock();
        }

        ignoreevents = false;
    }
}

void PlaybackBox::playSelected()
{
    killPlayer();

    if (!curitem || ignoreevents)
        return;
     
    play(curitem);
}

void PlaybackBox::deleteSelected()
{
    killPlayer();

    if (!curitem || ignoreevents)
        return;

    remove(curitem);
}

void PlaybackBox::expireSelected()
{
    killPlayer();

    if (!curitem || ignoreevents)
        return;

    expire(curitem);
}

void PlaybackBox::selected()
{
    killPlayer();

    if (!curitem || ignoreevents)
        return;

    switch (type) 
    {
        case Play: play(curitem); break;
        case Delete: remove(curitem); break;
    }
}

void PlaybackBox::play(ProgramInfo *rec)
{
    if (!rec || ignoreevents)
        return;

    ignoreevents = true;

    if (fileExists(rec) == false)
    {
        cerr << "Error: " << rec->pathname << " file not found\n";
        ignoreevents = false;
        killPlayer();
        return;
    }

    ProgramInfo *tvrec = new ProgramInfo(*rec);

    QSqlDatabase *db = QSqlDatabase::database();
    noUpdate = true;

    TV *tv = new TV(db);
    tv->Init();

    ignoreevents = true;
    if (tv->Playback(tvrec))
    {
        while (tv->GetState() != kState_None)
        {
            qApp->unlock();
            qApp->processEvents();
            usleep(50);
            qApp->lock();
        }
    }
    noUpdate = false;

    ignoreevents = false;

    update(fullRect);

    bool doremove = false;
    bool doprompt = false;

    if (tv->getRequestDelete())
    {
        doremove = true;
    }
    else if (tv->getEndOfRecording() &&
             gContext->GetNumSetting("EndOfRecordingExitPrompt"))
    {
        doprompt = true;
    }

    delete tv;

    if (doremove)
    {
        remove(tvrec);
    }
    else if (doprompt) 
    {
        promptEndOfRecording(tvrec);
    }

    delete tvrec;

    FillList();
    skipUpdate = false;
    update(fullRect);

    ignoreevents = false;

    timer->start(1000 / 30);
}

void PlaybackBox::doRemove(ProgramInfo *rec)
{
    noUpdate = true;
    RemoteDeleteRecording(rec);
    noUpdate = false;

    if (titleitems == 1)
    {
        inTitle = true;
        curTitle = 0;
        curShowing = 0;
        skipNum = 0;
        FillList();
    }
    else 
    {
        titleitems--;
        if (titleitems < listsize)
            listCount--;

        FillList();

        if (skipNum < 0)
            skipNum = 0;

        if (inTitle == false)
        {
            if ((int)(skipNum + curShowing) >= (int)(titleitems - 1))
            {
                skipNum = titleitems - listsize;
                curShowing = listsize - 1;
            }
            else if ((int)(skipNum + listsize) >= (int)titleitems)
            {
                skipNum = titleitems - listsize;
                if (skipNum >= 0)
                    curShowing++;
            }

            if (curShowing >= listCount)
                curShowing = listCount - 1;
        }
        else
        {
            if (curTitle >= (signed int)showList.count())
                curTitle = 0;

            if (curTitle < 0)
                curTitle = 0;

            while (titleData[curTitle] == "***FILLER***")
            {
                curTitle++;
                if (curTitle >= (signed int)showList.count())
                    curTitle = 0;
            }
        }
    }
    update(listRect);
}

void PlaybackBox::remove(ProgramInfo *toDel)
{
    if (ignoreevents)
        return;

    killPlayer();

    ignoreevents = true;

    delitem = new ProgramInfo(*toDel);
    showDeletePopup(2);
}

void PlaybackBox::expire(ProgramInfo *toExp)
{
    if (ignoreevents)
        return;

    killPlayer();

    ignoreevents = true;

    delitem = new ProgramInfo(*toExp);
    showDeletePopup(3);
}

void PlaybackBox::showDeletePopup(int types)
{
    QFont bigFont("Arial", (int)(gContext->GetBigFontSize() * hmult), 
                  QFont::Bold);
    QFont medFont("Arial", (int)(gContext->GetMediumFontSize() * hmult), 
                  QFont::Bold);
    QFont smallFont("Arial", (int)(gContext->GetSmallFontSize() * hmult), 
                    QFont::Bold);

    ignoreevents = true;

    QDateTime startts = delitem->startts;
    QDateTime endts = delitem->endts;

    QString timedate = startts.date().toString(dateformat) + QString(", ") +
                       startts.time().toString(timeformat) + QString(" - ") +
                       endts.time().toString(timeformat);

    QString descrip = delitem->description;
    descrip = cutDown(descrip, &medFont, (int)(width() / 2));
    QString titl = delitem->title;
    titl = cutDown(titl, &bigFont, (int)(width() / 2));

    timer->stop();
    playingVideo = false;

    backup.begin(this);
    grayOut(&backup);
    backup.end();
    noUpdate = true;

    popup = new MythPopupBox(gContext->GetMainWindow(), "delete popup");
    popup->setFrameStyle( QFrame::Box | QFrame::Plain );
    if (graphicPopup == false)
        popup->setPaletteBackgroundColor(popupBackground);
    else
        gContext->ThemeWidget(popup);
    popup->setPaletteForegroundColor(popupHighlight);
    QLabel *msg = NULL;
    if (types == 1)
        msg = new QLabel(tr("You have finished watching:"), popup);
    else if (types == 2)
        msg = new QLabel(tr("Are you sure you want to delete:"), popup);
    else if (types == 3)
        msg = new QLabel(tr("Allow this program to AutoExpire?"), popup);
    msg->setBackgroundOrigin(ParentOrigin); 
    msg->setPaletteForegroundColor(popupForeground);
    QLabel *filler1 = new QLabel("", popup);
    filler1->setBackgroundOrigin(ParentOrigin);
    QLabel *title = new QLabel(delitem->title, popup);
    title->setPaletteForegroundColor(popupForeground);
    title->setBackgroundOrigin(ParentOrigin);
    title->setFont(bigFont);
    title->setMaximumWidth((int)(width() / 2));
    QLabel *subtitle = new QLabel("\"" + delitem->subtitle + "\"", popup);
    subtitle->setPaletteForegroundColor(popupForeground);
    subtitle->setBackgroundOrigin(ParentOrigin);
    QLabel *times = new QLabel(timedate, popup);
    times->setPaletteForegroundColor(popupForeground);
    times->setBackgroundOrigin(ParentOrigin);
    QLabel *filler2 = new QLabel("", popup);
    filler2->setBackgroundOrigin(ParentOrigin);
    QLabel *msg2 = NULL;

    if (types == 1)
        msg2 = new QLabel(tr("Delete this recording?"), popup);
    else
        msg2 = new QLabel(" ", popup);

    msg2->setPaletteForegroundColor(popupForeground);
    msg2->setBackgroundOrigin(ParentOrigin);

    MythPushButton *yesButton;

    if ((types == 1) || (types == 2))
        yesButton = new MythPushButton(tr("Yes, get rid of it"), popup);
    else
        yesButton = new MythPushButton(tr("Yes, AutoExpire"), popup);

    MythPushButton *noButton = NULL;

    if (types == 1)
        noButton = new MythPushButton(tr("No, I might want to watch it again."),
                                      popup);
    else if (types == 2)
        noButton = new MythPushButton(tr("No, keep it, I changed my mind"),
                                      popup);
    else if (types == 3)
        noButton = new MythPushButton(tr("No, Do Not AutoExpire"), popup);

    popup->addWidget(msg, false);
    popup->addWidget(filler1, false);
    popup->addWidget(title, false);
    if ((delitem->subtitle).stripWhiteSpace().length() > 0)
        popup->addWidget(subtitle, false);
    else
        subtitle->hide();
    popup->addWidget(times, false);
    popup->addWidget(filler2, false);
    popup->addWidget(msg2, false);

    popup->addWidget(yesButton, false);
    popup->addWidget(noButton, false);

    if ((types == 1) || (types == 2))
        noButton->setFocus();
    else
    {
        QSqlDatabase *db = QSqlDatabase::database();
        if (delitem->GetAutoExpireFromRecorded(db))
            yesButton->setFocus();
        else
            noButton->setFocus();
    }
  
    msg->adjustSize();
    msg2->adjustSize();
    filler1->adjustSize();
    filler2->adjustSize();
    title->adjustSize();
    times->adjustSize();
    subtitle->adjustSize();
    yesButton->adjustSize();
    noButton->adjustSize();

    popup->polish();

    int x, y, maxw, poph;
    poph = msg->height() + msg2->height() + filler1->height() + 
           filler2->height() + title->height() + times->height() + 
           subtitle->height() + yesButton->height() + noButton->height() +
           (int)(110 * hmult);
    popup->setMinimumHeight(poph);
    maxw = 0;

    if (title->width() > maxw)
        maxw = title->width();
    if (times->width() > maxw)
        maxw = times->width();
    if (subtitle->width() > maxw)
        maxw = subtitle->width();
    if (noButton->width() > maxw)
        maxw = noButton->width();

    maxw += (int)(80 * wmult);
 
    x = (int)(width() / 2) - (int)(maxw / 2);
    y = (int)(height() / 2) - (int)(poph / 2);

    popup->setFixedSize(maxw, poph);
    popup->setGeometry(x, y, maxw, poph);

    popup->Show();

    if ((types == 1) || (types == 2))
    {
        connect(yesButton, SIGNAL(pressed()), this, SLOT(doDelete()));
        connect(noButton, SIGNAL(pressed()), this, SLOT(noDelete()));
    }
    else if (types == 3)
    {
        connect(yesButton, SIGNAL(pressed()), this, SLOT(doAutoExpire()));
        connect(noButton, SIGNAL(pressed()), this, SLOT(noAutoExpire()));
    }

    QAccel *popaccel = new QAccel(popup);
    popaccel->connectItem(popaccel->insertItem(Key_Escape), this, 
                          SLOT(noDelete()));
}

void PlaybackBox::noDelete()
{
    popup->hide();

    noUpdate = false;
    backup.begin(this);
    backup.drawPixmap(0, 0, myBackground);
    backup.end();

    ignoreevents = false;
    delete delitem;
    delete popup;
    popup = NULL;
    delitem = NULL;

    skipUpdate = false;
    skipCnt = 2;
    update(fullRect);

    setActiveWindow();

    timer->start(1000 / 30);
}

void PlaybackBox::doDelete()
{
    doRemove(delitem);

    popup->hide();

    noUpdate = false;
    backup.begin(this);
    backup.drawPixmap(0, 0, myBackground);
    backup.end();

    ignoreevents = false;

    delete popup;
    delete delitem;
    delitem = NULL;

    skipUpdate = false;
    skipCnt = 2;
    update(fullRect);

    setActiveWindow();

    timer->start(1000 / 30);
}

void PlaybackBox::noAutoExpire()
{
    QSqlDatabase *db = QSqlDatabase::database();
    delitem->SetAutoExpire(false, db);

    popup->hide();

    noUpdate = false;
    backup.begin(this);
    backup.drawPixmap(0, 0, myBackground);
    backup.end();

    ignoreevents = false;
    delete delitem;
    delete popup;
    popup = NULL;
    delitem = NULL;

    skipUpdate = false;
    skipCnt = 2;
    update(fullRect);

    setActiveWindow();

    timer->start(1000 / 30);
}

void PlaybackBox::doAutoExpire()
{
    QSqlDatabase *db = QSqlDatabase::database();
    delitem->SetAutoExpire(true, db);

    popup->hide();

    noUpdate = false;
    backup.begin(this);
    backup.drawPixmap(0, 0, myBackground);
    backup.end();

    ignoreevents = false;

    delete popup;
    delete delitem;
    delitem = NULL;

    skipUpdate = false;
    skipCnt = 2;
    update(fullRect);

    setActiveWindow();

    timer->start(1000 / 30);
}

void PlaybackBox::promptEndOfRecording(ProgramInfo *rec)
{
    if (!rec || ignoreevents)
        return;

    killPlayer();

    ignoreevents = true;

    if (!rec)
    {
        ignoreevents = false;
        return;
    }

    delitem = new ProgramInfo(*rec);
    showDeletePopup(1);
}

void PlaybackBox::UpdateProgressBar(void)
{
    update(usageRect);
}

void PlaybackBox::timeout(void)
{
    if (ignoreevents == true)
        return;
    if (noUpdate)
        return;
    if (showData.count() == 0)
        return;

    if (!nvp && playingVideo == false)
    {
        if (playbackPreview == 1)
        {
            if (curitem)
            {
                ProgramInfo *rec = curitem;

                if (fileExists(rec) == false)
                {
                    cerr << "Error: File Missing!\n";
                    QPixmap temp((int)(160 * wmult), (int)(120 * hmult));

                    killPlayer();
                    return;
                }

                startPlayer(rec);
            }
        }
    }

    update(videoRect);

    if (playbackPreview == 0)
        timer->stop();
}

void PlaybackBox::keyPressEvent(QKeyEvent *e)
{
    if (ignoreevents || noUpdate)
        return;

    bool handled = false;

    if (showData.count() > 0)
    {
        handled = true;
        switch (e->key())
        {
            case Key_D: 
                deleteSelected(); 
                break;
            case Key_I: 
                expireSelected(); 
                break;
            case Key_P: 
                playSelected(); 
                break;
            case Key_Space:
            case Key_Enter:
            case Key_Return:
                selected();
                break;
            default:  handled = false; break;
        }
    }

    if (handled)
        return;

    switch (e->key())
    {
        case Key_Left: cursorLeft(); break;
        case Key_Right: cursorRight(); break;
        case Key_Down: cursorDown(); break;
        case Key_Up: cursorUp(); break;
        case Key_3: case Key_PageUp: pageUp(); break;
        case Key_9: case Key_PageDown: pageDown(); break;
        case Key_Escape: exitWin(); break;
        default: MythDialog::keyPressEvent(e); break;
    }   
}

void PlaybackBox::customEvent(QCustomEvent *e)
{
    if (ignoreevents || noUpdate)
        return;

    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();
 
        if (message == "RECORDING_LIST_CHANGE")
        {
            if (QDateTime::currentDateTime() > lastUpdateTime.addSecs(1))
            {
                FillList();      
                update(fullRect);
            }
            if (type == Delete)
                UpdateProgressBar();
        }
    }
}

bool PlaybackBox::fileExists(ProgramInfo *pginfo)
{
    if (pginfo->pathname.left(7) == "myth://")
    {
        noUpdate = true;
        bool ret = RemoteCheckFile(pginfo);
        noUpdate = false;
        return ret;
    }

    QFile checkFile(pginfo->pathname);

    return checkFile.exists();
}

QPixmap PlaybackBox::getPixmap(ProgramInfo *pginfo)
{
    QPixmap *tmppix;
    QPixmap retpixmap;

    if (gContext->GetNumSetting("GeneratePreviewPixmaps") != 1)
        return retpixmap;

    QString filename = pginfo->pathname;
    filename += ".png";

    int screenheight = 0, screenwidth = 0;
    float wmult = 0, hmult = 0;

    noUpdate = true;
    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    tmppix = gContext->LoadScalePixmap(filename);
    if (tmppix)
    {
        retpixmap = *tmppix;
        delete tmppix;
        noUpdate = false;
        return retpixmap;
    }

    QImage *image = gContext->CacheRemotePixmap(filename);

    if (!image)
    {
        RemoteGeneratePreviewPixmap(pginfo);

        tmppix = gContext->LoadScalePixmap(filename);
        if (tmppix)
        {
            retpixmap = *tmppix;
            delete tmppix;
            noUpdate = false;
            return retpixmap;
        }

        image = gContext->CacheRemotePixmap(filename);
    }
    noUpdate = false;

    if (image)
    {
        tmppix = new QPixmap();

        if (screenwidth != 800 || screenheight != 600)
        {
            QImage tmp2 = image->smoothScale((int)(image->width() * wmult),
                                             (int)(image->height() * hmult));
            tmppix->convertFromImage(tmp2);
        }
        else
        {
            tmppix->convertFromImage(*image);
        }
    }
 
    if (!tmppix)
    {
        QPixmap tmp((int)(160 * wmult), (int)(120 * hmult));
        tmp.fill(black);
        return tmp;
    }

    retpixmap = *tmppix;
    delete tmppix;
    return retpixmap;
}

