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
#include <qmap.h>

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
    expectingPopup = false;

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

    firstrun = true;

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

    timer->start(500);
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

    QMap<QString, QString> regexpMap;
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

        curitem->ToMap(m_db, regexpMap);

        LayerSet *container = NULL;
        if (type != Delete)
            container = theme->GetSet("program_info_play");
        else
            container = theme->GetSet("program_info_del");
        if (container)
        {
            if ((playbackPreview == 0) &&
                (generatePreviewPixmap == 0))
                container->UseAlternateArea(true);

            container->ClearAllText();
            container->SetTextByRegexp(regexpMap);

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

        timer->start(500);
    }
    else if (!firstrun)
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

    if (((playbackPreview == 0) || !playingVideo) && curitem)
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
        VideoFrame *frame = nvp->GetCurrentFrame(w, h);

        if (w == 0 || h == 0 || !frame || !(frame->buf))
            return;

        unsigned char *buf = frame->buf;

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

    int total = 0, used = 0;
    noUpdate = true;
    if (connected)
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
    int tempCurrent = 0;

    QString match;
    QRect pr = listRect;
    QPixmap pix(pr.size());

    LayerSet *container = NULL;
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    container = theme->GetSet("selector");

    typedef QMap<QString, ProgramInfo> ShowData;
    ProgramInfo *tempInfo;

    ShowData::Iterator it;
    ShowData::Iterator start;
    ShowData::Iterator end;
    if (titleData && titleData[curTitle] != tr("All Programs"))
    {
        start = showData.begin();
        end = showData.end();
    }
    else
    {
        start = showDateData.begin();
        end = showDateData.end();
    }

    int overrectime = gContext->GetNumSetting("RecordOverTime", 0);
    int underrectime = gContext->GetNumSetting("RecordPreRoll", 0);

    if (container && titleData)
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

                         tempCurrent = RemoteGetRecordingStatus(tempInfo,
                                                                overrectime,
                                                                underrectime);

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
                         if (tempCurrent == 1)
                             ltype->EnableForcedFont(cnt, "recording");
                         else if (tempCurrent > 1)
                             ltype->EnableForcedFont(cnt, "recording"); // FIXME: change to overunderrecording, fall back to recording. 

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

    if (showData.count() == 0 && !firstrun)
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

bool PlaybackBox::FillList()
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

    return (infoList != NULL);
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
            {
                cerr << "Took too long to start playing\n";
                break;
            }
            qApp->unlock();
            qApp->processEvents();
            usleep(50);
            qApp->lock();
        }
        curtime = QDateTime::currentDateTime();
        curtime = curtime.addSecs(2);

        rbuffer->Pause();
        while (!rbuffer->isPaused())
        {
            if (QDateTime::currentDateTime() > curtime)
            {
                cerr << "Took too long to pause ringbuffer\n";
                break;
            }
            qApp->unlock();
            qApp->processEvents();
            usleep(50);
            qApp->lock();
        }

        nvp->StopPlaying();

        while (nvp->IsPlaying())
        {
            if (QDateTime::currentDateTime() > curtime)
            {
                cerr << "Took too long to stop playing\n";
                break;
            }
            qApp->unlock();
            qApp->processEvents();
            usleep(50);
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

void PlaybackBox::stopSelected()
{
    killPlayer();

    if (!curitem || ignoreevents)
        return;

    stop(curitem);
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

void PlaybackBox::showActionsSelected()
{
    killPlayer();

    if (!curitem || ignoreevents)
        return;

    showActions(curitem);
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

    connected = FillList();
    skipUpdate = false;
    update(fullRect);

    ignoreevents = false;

    timer->start(500);
}

void PlaybackBox::stop(ProgramInfo *rec)
{
    if (noUpdate)
        return;

    noUpdate = true;
    RemoteStopRecording(rec);
    noUpdate = false;
}

void PlaybackBox::doRemove(ProgramInfo *rec)
{
    if (noUpdate)
        return;

    noUpdate = true;
    RemoteDeleteRecording(rec);
    noUpdate = false;

    if (titleitems == 1)
    {
        inTitle = true;
        curTitle = 0;
        curShowing = 0;
        skipNum = 0;
        connected = FillList();
    }
    else 
    {
        titleitems--;
        if (titleitems < listsize)
            listCount--;

        connected = FillList();

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
    update(fullRect);
}

void PlaybackBox::remove(ProgramInfo *toDel)
{
    if (ignoreevents)
        return;

    killPlayer();

    ignoreevents = true;

    delitem = new ProgramInfo(*toDel);
    showDeletePopup(delitem, 2);
}

void PlaybackBox::expire(ProgramInfo *toExp)
{
    if (ignoreevents)
        return;

    killPlayer();

    ignoreevents = true;

    delitem = new ProgramInfo(*toExp);
    showDeletePopup(delitem, 3);
}

void PlaybackBox::showActions(ProgramInfo *toExp)
{
    if (ignoreevents)
        return;

    killPlayer();

    ignoreevents = true;

    delitem = new ProgramInfo(*toExp);
    showActionPopup(delitem);
}

void PlaybackBox::showDeletePopup(ProgramInfo *program, int types)
{
    ignoreevents = true;

    timer->stop();
    playingVideo = false;

    backup.begin(this);
    grayOut(&backup);
    backup.end();
    noUpdate = true;

    popup = new MythPopupBox(gContext->GetMainWindow(), graphicPopup,
                             popupForeground, popupBackground,
                             popupHighlight, "delete popup");
    QString message1;
    switch (types)
    {
        case 1: message1 = tr("You have finished watching:"); break;
        case 2: message1 = tr("Are you sure you want to delete:"); break;
        case 3: message1 = tr("Allow this program to AutoExpire?"); break;
        case 4: message1 = tr("Are you sure you want to stop:"); break;
        default: message1 = "ERROR ERROR ERROR"; break;
    }
    
    QString message2 = " ";
    if (types == 1)
        message2 = tr("Delete this recording?");
        
    initPopup(popup, program, message1, message2);

    QString tmpmessage;
    const char *tmpslot;

    switch (types)
    {
        case 1: case 2: tmpmessage = tr("Yes, get rid of it"); 
                        tmpslot = SLOT(doDelete());
                        break;
        case 3: tmpmessage = tr("Yes, AutoExpire"); 
                tmpslot = SLOT(doAutoExpire());
                break;
        case 4: tmpmessage = tr("Yes, stop recording it"); 
                tmpslot = SLOT(doStop());
                break;
        default: tmpmessage = "ERROR ERROR ERROR"; tmpslot = NULL; break;
    }
    QButton *yesButton = popup->addButton(tmpmessage, this, tmpslot);

    switch (types)
    {
        case 1: tmpmessage = tr("No, I might want to watch it again."); 
                tmpslot = SLOT(noDelete());
                break;
        case 2: tmpmessage = tr("No, keep it, I changed my mind"); 
                tmpslot = SLOT(noDelete());
                break;
        case 3: tmpmessage = tr("No, do not AutoExpire"); 
                tmpslot = SLOT(noAutoExpire());
                break;
        case 4: tmpmessage = tr("No, continue recording it"); 
                tmpslot = SLOT(noStop());
                break;
        default: tmpmessage = "ERROR ERROR ERROR"; tmpslot = NULL; break;
    }
    QButton *noButton = popup->addButton(tmpmessage, this, tmpslot);

    if (types == 1 || types == 2)
        noButton->setFocus();
    else
    {
        QSqlDatabase *db = QSqlDatabase::database();
        if (program->GetAutoExpireFromRecorded(db))
            yesButton->setFocus();
        else
            noButton->setFocus();
    }
 
    popup->ShowPopup(this, SLOT(doCancel())); 

    expectingPopup = true;
}

void PlaybackBox::showActionPopup(ProgramInfo *program)
{
    ignoreevents = true;

    timer->stop();
    playingVideo = false;

    backup.begin(this);
    grayOut(&backup);
    backup.end();
    noUpdate = true;

    popup = new MythPopupBox(gContext->GetMainWindow(), graphicPopup,
                             popupForeground, popupBackground,
                             popupHighlight, "action popup");

    initPopup(popup, program, " ", tr("Select action:"));

    QSqlDatabase *db = QSqlDatabase::database();

    QButton *playButton = popup->addButton(tr("Play"), this, SLOT(doPlay()));

    int overrectime = gContext->GetNumSetting("RecordOverTime", 0);
    int underrectime = gContext->GetNumSetting("RecordPreRoll", 0);

    if (RemoteGetRecordingStatus(program, overrectime, underrectime) > 0)
        popup->addButton(tr("Stop Recording"), this, SLOT(askStop()));

    if (delitem->GetAutoExpireFromRecorded(db))
        popup->addButton(tr("Don't Auto Expire"), this, SLOT(noAutoExpire()));
    else
        popup->addButton(tr("Auto Expire"), this, SLOT(doAutoExpire()));

    popup->addButton(tr("Delete"), this, SLOT(askDelete()));
    popup->addButton(tr("Cancel"), this, SLOT(doCancel()));

    popup->ShowPopup(this, SLOT(doCancel()));

    playButton->setFocus();

    expectingPopup = true;
}

void PlaybackBox::initPopup(MythPopupBox *popup, ProgramInfo *program,
                            QString message, QString message2)
{
    QDateTime startts = program->startts;
    QDateTime endts = program->endts;

    QString timedate = startts.date().toString(dateformat) + QString(", ") +
                       startts.time().toString(timeformat) + QString(" - ") +
                       endts.time().toString(timeformat);

    QString descrip = program->description;
    descrip = cutDownString(descrip, &defaultMediumFont, (int)(width() / 2));
    QString titl = program->title;
    titl = cutDownString(titl, &defaultBigFont, (int)(width() / 2));

    if (message.stripWhiteSpace().length() > 0)
    {
        popup->addLabel(message);
        popup->addLabel("");
    }

    popup->addLabel(program->title, MythPopupBox::Large);

    if ((program->subtitle).stripWhiteSpace().length() > 0)
        popup->addLabel("\"" + program->subtitle + "\"");

    popup->addLabel(timedate);

    if (message2.stripWhiteSpace().length() > 0)
    {
        popup->addLabel("");
        popup->addLabel(message2);
    }
}

void PlaybackBox::cancelPopup(void)
{
    popup->hide();
    expectingPopup = false;

    noUpdate = false;
    backup.begin(this);
    backup.drawPixmap(0, 0, myBackground);
    backup.end();

    ignoreevents = false;

    delete popup;
    popup = NULL;

    skipUpdate = false;
    skipCnt = 2;
    update(fullRect);

    setActiveWindow();
}

void PlaybackBox::doPlay(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    play(delitem);

    delete delitem;
    delitem = NULL;
}

void PlaybackBox::askStop(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    showDeletePopup(delitem, 4);
}

void PlaybackBox::noStop(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    delete delitem;
    delitem = NULL;

    timer->start(500);
}

void PlaybackBox::doStop(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    stop(delitem);

    delete delitem;
    delitem = NULL;

    timer->start(500);
}

void PlaybackBox::askDelete(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    showDeletePopup(delitem, 2);
}

void PlaybackBox::noDelete(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    delete delitem;
    delitem = NULL;

    timer->start(500);
}

void PlaybackBox::doDelete(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    doRemove(delitem);

    delete delitem;
    delitem = NULL;

    timer->start(500);
}

void PlaybackBox::noAutoExpire(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    QSqlDatabase *db = QSqlDatabase::database();
    delitem->SetAutoExpire(false, db);

    delete delitem;
    delitem = NULL;
}

void PlaybackBox::doAutoExpire(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    QSqlDatabase *db = QSqlDatabase::database();
    delitem->SetAutoExpire(true, db);

    delete delitem;
    delitem = NULL;
}

void PlaybackBox::doCancel(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    delete delitem;
    delitem = NULL;
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
    showDeletePopup(delitem, 1);
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

    if (firstrun)
    {
        firstrun = false;
        connected = FillList();
        skipUpdate = false;
        repaint(fullRect);
    }

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
    else
        timer->start(1000 / 30);
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
                showActionsSelected(); 
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
                connected = FillList();      
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

