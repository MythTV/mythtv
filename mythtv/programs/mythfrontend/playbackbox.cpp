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
#include "infodialog.h"
#include "remoteutil.h"

PlaybackBox::PlaybackBox(BoxType ltype, MythMainWindow *parent, 
                         const char *name)
           : MythDialog(parent, name)
{
    type = ltype;
    
    state = kStopped;
    killState = kDone;
    waitToStart = false;
    enableGrayOut = false;

    rbuffer = NULL;
    nvp = NULL;

    // Number of items available in list, showData.count() for "All Programs"
    // In other words, this is the number of shows selected by the title selec.
    titleitems = 0;         
    
    // How full the actual list is (list in this context being the number
    // of spaces available for data to be shown).
    listCount = 0;          

    inTitle = gContext->GetNumSetting("PlaybackBoxStartInTitle", 0);

    transparentFlag = gContext->GetNumSetting("PlayBoxShading", 0);

 
    skipNum = 0;            // Amount of records to skip (for scrolling)

    curShowing = 0;         // Where in the list (0 - # in list)
    pageDowner = false;     // Is there enough records to page down?
    skipUpdate = false;
    skipCnt = 0;
    
    previewPixmap = NULL;
    previewProgamInfo = NULL;
    
    updateFreeSpace = true;
    freeSpaceTotal = 0;
    freeSpaceUsed = 0;

    leftRight = false;      // If change is left or right, don't restart video
    playingVideo = false;
    graphicPopup = true;
    expectingPopup = false;

    titleData = NULL;
    popup = NULL;
    curitem = NULL;
    delitem = NULL;


    groupnameAsAllProg = gContext->GetNumSetting("DispRecGroupAsAllProg", 0);
    listOrder = gContext->GetNumSetting("PlayBoxOrdering", 1);    
    recGroupType.clear();

    curGroupPassword = QString("");
    recGroup = gContext->GetSetting("DisplayRecGroup", QString("All Programs"));
    VERBOSE( VB_GENERAL, recGroup);
    if(!groupnameAsAllProg)
        groupDisplayName = tr("All Programs");
    else
        groupDisplayName = recGroup;

    recGroupPassword = getRecGroupPassword(recGroup);

    if (gContext->GetNumSetting("DisplayRecGroupIsCategory",0))
        recGroupType[recGroup] = "category";
    else
        recGroupType[recGroup] = "recgroup";

    fullRect = QRect(0, 0, size().width(), size().height());
    listRect = QRect(0, 0, 0, 0);
    infoRect = QRect(0, 0, 0, 0);
    usageRect = QRect(0, 0, 0, 0);
    videoRect = QRect(0, 0, 0, 0);

    showDateFormat = gContext->GetSetting("ShortDateFormat", "M/d");
    showTimeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");
    overrectime = gContext->GetNumSetting("RecordOverTime", 0);
    underrectime = gContext->GetNumSetting("RecordPreRoll", 0);

    
    bgTransBackup = gContext->LoadScalePixmap("trans-backup.png");

    if (!bgTransBackup)
        bgTransBackup = new QPixmap();

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    theme->LoadTheme(xmldata, "playback");

    LoadWindow(xmldata);

    LayerSet *container = theme->GetSet("selector");
    UIListType *listtype = NULL;
    if (container)
    {
        listtype = (UIListType *)container->GetType("showing");
        if (listtype)
            listsize = listtype->GetItems();
    }
    else
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), QObject::tr("Failed to get selector object"),
                                  QObject::tr("Myth could not locate the selector object within your "
                                  "theme.\nPlease make that your ui.xml is valid.\n\nMyth will now exit."));
                                  
        cerr << "Failed to get selector object.\n";
        exit(0);
        return;
    }

    if(theme->GetSet("group_info") && gContext->GetNumSetting("ShowGroupInfo", 0) == 1)
    {
        haveGroupInfoSet = true;
        if(listtype)
            listtype->ShowSelAlways(false);
    }
    else
        haveGroupInfoSet = false;


    connected = FillList();

    curTitle = 0;

    playbackPreview = gContext->GetNumSetting("PlaybackPreview");
    generatePreviewPixmap = gContext->GetNumSetting("GeneratePreviewPixmaps");
    dateformat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");

    nvp = NULL;
    timer = new QTimer(this);

    updateBackground();

    setNoErase();

    connect(timer, SIGNAL(timeout()), this, SLOT(timeout()));

    timer->start(500);
    gContext->addListener(this);

    freeSpaceTimer = new QTimer(this);
    connect(freeSpaceTimer, SIGNAL(timeout()), this, 
            SLOT(setUpdateFreeSpace()));

    if (recGroupPassword != "")
        showRecGroupChooser();
}

PlaybackBox::~PlaybackBox(void)
{
    gContext->removeListener(this);
    killPlayerSafe();
    delete timer;
    delete theme;
    delete bgTransBackup;
    
    if(previewPixmap)
        delete previewPixmap;
        
    if (curitem)
        delete curitem;
    if(delitem)
        delete delitem;
        
    if (titleData)
        delete [] titleData;
}

/* blocks until playing has stopped */
void PlaybackBox::killPlayerSafe(void)
{
    /* don't process any keys while we are trying to let the nvp stop */
    /* if the user keeps selecting new recordings we will never stop playing */
    setEnabled(false);

    if (state != kKilled && state != kStopped && playbackPreview != 0)
    {
        while (state != kKilled)
        {
            /* ensure that key events don't mess up our states */
            if((state != kKilling) && (state != kKilled))
                state = kKilling;

            /* NOTE: need unlock/process/lock here because we need
               to allow updateVideo() to run to handle changes in states */
            qApp->unlock();
            qApp->processEvents();
            usleep(500);
            qApp->lock();
        }
    }

    setEnabled(true);
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
    killPlayerSafe();
    accept();
}

void PlaybackBox::updateBackground(void)
{
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
    if (r.intersects(videoRect))
    {
        updateVideo(&p);
    }


    // the grayouting needs to be cleared when exiting the popup...
    if (popup != NULL || enableGrayOut)
    {
        backup.begin(this);
        grayOut(&backup);
        backup.end();
    }

    skipCnt--;
    if (skipCnt < 0)
    {
        skipUpdate = true;
        skipCnt = 0;
    }
}

void PlaybackBox::grayOut(QPainter *tmp)
{
    //int transparentFlag = gContext->GetNumSetting("PlayBoxShading", 0);
    if (transparentFlag == 0)
        tmp->fillRect(QRect(QPoint(0, 0), size()), 
                      QBrush(QColor(10, 10, 10), Dense4Pattern));
    else if (transparentFlag == 1)
        tmp->drawPixmap(0, 0, *bgTransBackup, 0, 0, (int)(800*wmult), 
                        (int)(600*hmult));
}

void PlaybackBox::updateGroupInfo(QPainter *p, QRect& pr, QPixmap& pix)
{
    if (haveGroupInfoSet)
    {
        LayerSet *container = theme->GetSet("group_info");
        container->ClearAllText();
        QPainter tmp(&pix);
        QMap<QString, QString> infoMap;
        if(titleData[curTitle] == groupDisplayName)
            infoMap["title"] = titleData[curTitle];
        else
            infoMap["title"] = QString("%1 - %2").arg(groupDisplayName)
                                                 .arg(titleData[curTitle]);
        
        if( countInGroup > 1  )
            infoMap["description"] = QString(tr("There are %1 recordings in "
                                                "this display group")).arg(countInGroup);
        else if( countInGroup == 1  )
            infoMap["description"] = QString(tr("There is one recording in "
                                                 "this display group"));
        else
            infoMap["description"] = QString(tr("There are no recordings in "
                                                "this display group"));
        container->ClearAllText();
        container->SetText(infoMap);

        if (type != Delete)
            container->Draw(&tmp, 6, 0);
        else
            container->Draw(&tmp, 6, 1);

        tmp.end();
        p->drawPixmap(pr.topLeft(), pix);
    }
    else
        updateProgramInfo(p, pr, pix);

}


void PlaybackBox::updateProgramInfo(QPainter *p, QRect& pr, QPixmap& pix)
{
    QSqlDatabase *m_db = QSqlDatabase::database();
    QMap<QString, QString> infoMap;
    QPainter tmp(&pix);
        
    if (playingVideo == true)
        state = kChanging;

    

    LayerSet *container = NULL;
    if (type != Delete)
        container = theme->GetSet("program_info_play");
    else
        container = theme->GetSet("program_info_del");

    if (container)
    {
        curitem->ToMap(m_db, infoMap);
        
        if ((playbackPreview == 0) &&
            (generatePreviewPixmap == 0))
            container->UseAlternateArea(true);

        container->ClearAllText();
        container->SetText(infoMap);

        int flags = curitem->programflags;

        UIImageType *itype;
        itype = (UIImageType *)container->GetType("commflagged");
        if (itype)
        {
            if (flags & FL_COMMFLAG)
                itype->ResetFilename();
            else
                itype->SetImage("blank.png");
            itype->LoadImage();
        }

        itype = (UIImageType *)container->GetType("cutlist");
        if (itype)
        {
            if (flags & FL_CUTLIST)
                itype->ResetFilename();
            else
                itype->SetImage("blank.png");
            itype->LoadImage();
        }

        itype = (UIImageType *)container->GetType("autoexpire");
        if (itype)
        {
            if (flags & FL_AUTOEXP)
                itype->ResetFilename();
            else
                itype->SetImage("blank.png");
            itype->LoadImage();
        }

        itype = (UIImageType *)container->GetType("processing");
        if (itype)
        {
            if (flags & FL_EDITING)
                itype->ResetFilename();
            else
                itype->SetImage("blank.png");
            itype->LoadImage();
        }

        itype = (UIImageType *)container->GetType("bookmark");
        if (itype)
        {
            if (flags & FL_BOOKMARK)
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

    waitToStartPreviewTimer.start();
    waitToStart = true;

}

void PlaybackBox::updateInfo(QPainter *p)
{
    QRect pr = infoRect;
    QPixmap pix(pr.size());
    bool updateGroup = (inTitle && haveGroupInfoSet);
    pix.fill(this, pr.topLeft());
    

    if (showData.count() > 0 && curitem && !updateGroup)
    {
        updateProgramInfo(p, pr, pix);
    }
    else if(updateGroup)
    {
        updateGroupInfo(p, pr, pix);
    }
    else
    {
        QPainter tmp(&pix);
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
    // If we're displaying group info don't update the video.
    if (inTitle && haveGroupInfoSet)
        return;

    /* show a still frame if the user doesn't want a video preview or nvp 
     * hasn't started playing the video preview yet */
    if (((playbackPreview == 0) || !playingVideo || (state == kStarting) || 
        (state == kChanging)) && curitem)
    {
        QPixmap temp = getPixmap(curitem);
        if (temp.width() > 0)
        {
            p->drawPixmap(videoRect.x(), videoRect.y(), temp);
        }
    }

    /* keep calling killPlayer() to handle nvp cleanup */
    /* until killPlayer() is done */
    if (killState != kDone)
    {
        if (!killPlayer())
            return;
    }

    /* if we aren't supposed to have a preview playing then always go */
    /* to the stopping state */
    if (!playbackPreview && (state != kKilling) && (state != kKilled))
    {
        state = kStopping;
    }

    /* if we have no nvp and aren't playing yet */
    /* if we have an item we should start playing */
    if (!nvp && playbackPreview && (playingVideo == false) && curitem && 
        (state != kKilling) && (state != kKilled) && (state != kStarting))
    {
        ProgramInfo *rec = curitem;

        if (fileExists(rec) == false)
        {
            cerr << "Error: File missing\n";

            state = kStopping;
            return;
        }
        state = kStarting;
    }

    if (state == kChanging)
    {
        if (nvp)
        {
            killPlayer(); /* start killing the player */
            return;
        }

        state = kStarting;
    }

    if ((state == kStarting) && 
        (!waitToStart || (waitToStartPreviewTimer.elapsed() > 500)))
    {
        waitToStart = false;

        if (!nvp)
            startPlayer(curitem);

        if (nvp->IsPlaying())
            state = kPlaying;
    }

    if ((state == kStopping) || (state == kKilling))
    {
        if (nvp)
        {
            killPlayer(); /* start killing the player and exit */
            return;
        }

        if (state == kKilling)
            state = kKilled;
        else
            state = kStopped;
    }

    /* if we are playing and nvp is running, then grab a new video frame */
    if ((state == kPlaying) && nvp->IsPlaying())
    {
        int w = 0, h = 0;
        VideoFrame *frame = nvp->GetCurrentFrame(w, h);

        if (w == 0 || h == 0 || !frame || !(frame->buf))
        {
            nvp->ReleaseCurrentFrame(frame);
            return;
        }

        unsigned char *buf = frame->buf;

        unsigned char *outputbuf = new unsigned char[w * h * 4];
        yuv2rgb_fun convert = yuv2rgb_init_mmx(32, MODE_RGB);

        convert(outputbuf, buf, buf + (w * h), buf + (w * h * 5 / 4), w, h, 
                w * 4, w, w / 2, 0);

        nvp->ReleaseCurrentFrame(frame);

        QImage img(outputbuf, w, h, 32, NULL, 65536 * 65536, 
                   QImage::LittleEndian);
        img = img.scale(videoRect.width(), videoRect.height());

        delete [] outputbuf;

        p->drawImage(videoRect.x(), videoRect.y(), img);
    }

    /* have we timed out waiting for nvp to start? */
    if ((state == kPlaying) && !nvp->IsPlaying())
    {
        if (nvpTimeout.elapsed() > 2000)
        {
            state = kStarting;
            killPlayer();
            return;
        }
    }
}

void PlaybackBox::updateUsage(QPainter *p)
{
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

        if (updateFreeSpace && connected)
        {
            updateFreeSpace = false;
            RemoteGetFreeSpace(freeSpaceTotal, freeSpaceUsed);
            freeSpaceTimer->start(15000);
        }    

        QString usestr;

        double perc = (double)((double)freeSpaceUsed / (double)freeSpaceTotal);
        perc = ((double)100 * (double)perc);
        usestr.sprintf("%d", (int)perc);
        usestr = usestr + tr("% used");

        QString size;
        size.sprintf("%0.2f", (freeSpaceTotal - freeSpaceUsed) / 1024.0);
        QString rep = tr(", %1 GB free").arg(size);
        usestr = usestr + rep;

        QRect pr = usageRect;
        QPixmap pix(pr.size());
        pix.fill(this, pr.topLeft());
        QPainter tmp(&pix);

        UITextType *ttype = (UITextType *)container->GetType("freereport");
        if (ttype)
            ttype->SetText(usestr);

        UIStatusBarType *sbtype = 
                               (UIStatusBarType *)container->GetType("usedbar");
        if (sbtype)
        {
            sbtype->SetUsed(freeSpaceUsed);
            sbtype->SetTotal(freeSpaceTotal);
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
    if (titleData && titleData[curTitle] != groupDisplayName)
    {
        start = showData.begin();
        end = showData.end();
    }
    else
    {
        start = showDateData.begin();
        end = showDateData.end();
    }

    

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
                     || titleData[curTitle] == groupDisplayName)
                 {
                     if (pastSkip <= 0)
                     {
                         tempInfo = &(it.data());

                         tempCurrent = RemoteGetRecordingStatus(tempInfo,
                                                                overrectime,
                                                                underrectime);

                         if (titleData[curTitle] == groupDisplayName)
                             tempSubTitle = tempInfo->title; 
                         else
                              tempSubTitle = tempInfo->subtitle;
                           if (tempSubTitle.stripWhiteSpace().length() == 0)
                             tempSubTitle = tempInfo->title;
                         if ((tempInfo->subtitle).stripWhiteSpace().length() > 0 
                             && titleData[curTitle] == groupDisplayName)
                         {
                             tempSubTitle = tempSubTitle + " - \"" + 
                                            tempInfo->subtitle + "\"";
                         }

                         tempDate = (tempInfo->startts).toString(showDateFormat);
                         tempTime = (tempInfo->startts).toString(showTimeFormat);

                         long long size = tempInfo->filesize;
                         tempSize.sprintf("%0.2f GB",
                                          size / 1024.0 / 1024.0 / 1024.0);

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
                    titleData[curTitle] == groupDisplayName)
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
    if (titleData && titleData[curTitle] != groupDisplayName)
        countInGroup = cnt;
    else
        countInGroup = showData.size();

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

void PlaybackBox::cursorLeft()
{
    if(!inTitle)
    {
        if(haveGroupInfoSet)
            killPlayerSafe();
        inTitle = true;
        skipUpdate = false;
        
        update(fullRect);
        leftRight = true;
    }
    else
        exitWin();
        
}

void PlaybackBox::cursorRight()
{
    if(inTitle)
    {
        leftRight = true;
        inTitle = false;
        skipUpdate = false;
        update(fullRect);
    }
    else
        showActionsSelected();    
}

void PlaybackBox::cursorDown(bool page, bool newview)
{
    if (inTitle == true || newview)
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

        if (newview)
            inTitle = false;

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

void PlaybackBox::cursorUp(bool page, bool newview)
{
    if (inTitle == true || newview)
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

        if (newview)
            inTitle = false;

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
    
    
    int cnt = 999;
    if (listOrder == 0 && type != Delete)
        cnt = 100;

    showData.clear();
    showDateData.clear();
    if (showList.count() > 0)
    {
        showList.clear();
    }

    if (titleData)
        delete [] titleData;
    titleData = NULL;

    showList[""] = groupDisplayName;

    fillRecGroupPasswordCache();

    vector<ProgramInfo *> *infoList;
    infoList = RemoteGetRecordedList(type == Delete);
    if (infoList)
    {
        QString temp;
        
        
        vector<ProgramInfo *>::iterator i = infoList->begin();
        for (; i != infoList->end(); i++)
        {
            if (((((*i)->recgroup == recGroup) ||
                  (recGroup == tr("All Programs"))) &&
                 (recGroupPassword == curGroupPassword)) ||
                ((recGroupType[recGroup] == "category") &&
                 ((*i)->category == recGroup ) &&
                 ( !recGroupPwCache.contains((*i)->recgroup))))
            {
                showList[((*i)->title).lower()] = (*i)->title;
                temp = QString("%1-!-%2").arg(((*i)->title).lower()).arg(cnt);
                showData[temp] = *(*i);
                temp = QString("%1").arg(cnt);
                showDateData[temp] = *(*i);
                if (listOrder == 0 && type != Delete)
                    cnt++;
                else 
                    cnt--;
            }
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

    return (infoList != NULL);
}

static void *SpawnDecoder(void *param)
{
    NuppelVideoPlayer *nvp = (NuppelVideoPlayer *)param;
    nvp->StartPlaying();
    return NULL;
}

bool PlaybackBox::killPlayer(void)
{
    playingVideo = false;

    /* if we don't have nvp to deal with then we are done */
    if (!nvp)
    {
        killState = kDone;
        return true;
    }

    if (killState == kDone)
    {
        killState = kNvpToPlay;
        killTimeout.start();
    }
 
    if (killState == kNvpToPlay)
    {
        if (nvp->IsPlaying() || (killTimeout.elapsed() > 2000))
        {
            killState = kNvpToStop;

            rbuffer->Pause();
            nvp->StopPlaying();
        }
        else /* return false status since we aren't done yet */
            return false;
    }

    if (killState == kNvpToStop)
    {
        if (!nvp->IsPlaying() || (killTimeout.elapsed() > 2000))
        {
            pthread_join(decoder, NULL);
            delete nvp;
            delete rbuffer;

            nvp = NULL;
            rbuffer = NULL;
            killState = kDone;
        }
        else /* return false status since we aren't done yet */
            return false;
    }

    return true;
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

        rbuffer = new RingBuffer(rec->pathname, false, false);

        nvp = new NuppelVideoPlayer();
        nvp->SetRingBuffer(rbuffer);
        nvp->SetAsPIP();
        nvp->SetOSDFontName(gContext->GetSetting("OSDFont"),
                            gContext->GetSetting("OSDCCFont"),
                            gContext->GetInstallPrefix());
        QString filters = "";
        nvp->SetVideoFilters(filters);

        pthread_create(&decoder, NULL, SpawnDecoder, nvp);

        nvpTimeout.start();

        state = kStarting;
        timer->start(1000 / 30); 
    }
}

void PlaybackBox::playSelected()
{
    state = kStopping;

    if (!curitem)
        return;
     
    play(curitem);
}

void PlaybackBox::stopSelected()
{
    state = kStopping;

    if (!curitem)
        return;

    stop(curitem);
}

void PlaybackBox::deleteSelected()
{
    state = kStopping;

    if (!curitem)
        return;

    remove(curitem);
}

void PlaybackBox::expireSelected()
{
    state = kStopping;

    if (!curitem)
        return;

    expire(curitem);
}

void PlaybackBox::selected()
{
    state = kStopping;

    if(inTitle && haveGroupInfoSet)
    {
        cursorRight();
        return;
    }    

    if (!curitem)
        return;

    switch (type) 
    {
        case Play: play(curitem); break;
        case Delete: remove(curitem); break;
    }
}

void PlaybackBox::showMenu()
{
    //state = kStopping;
    //killPlayer();

    //timer->stop();
    //playingVideo = false;
    stopPreview();

    popup = new MythPopupBox(gContext->GetMainWindow(), graphicPopup,
                             popupForeground, popupBackground,
                             popupHighlight, "menu popup");

    QLabel *label = popup->addLabel(tr("Recording List Menu"),
                                  MythPopupBox::Large, false);
    label->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    QButton *topButton = popup->addButton(tr("Change Group View"), this,
                     SLOT(showRecGroupChooser()));

    if (recGroupType[recGroup] == "recgroup")
        popup->addButton(tr("Change Group Password"), this,
                         SLOT(showRecGroupPasswordChanger()));

    popup->addButton(tr("Cancel"), this, SLOT(doCancel()));

    popup->ShowPopup(this, SLOT(doCancel()));

    topButton->setFocus();

    expectingPopup = true;
}

void PlaybackBox::showActionsSelected()
{
    if (!curitem)
        return;

    if(inTitle && haveGroupInfoSet)
        return;

    showActions(curitem);
}

void PlaybackBox::play(ProgramInfo *rec)
{
    if (!rec)
        return;

    if (fileExists(rec) == false)
    {
        cerr << "Error: " << rec->pathname << " file not found\n";
        return;
    }

    ProgramInfo *tvrec = new ProgramInfo(*rec);

    TV *tv = new TV();
    tv->Init();

    setEnabled(false);
    state = kKilling; // stop preview playback and don't restart it

    if (tv->Playback(tvrec))
    {
        while (tv->GetState() != kState_None)
        {
            qApp->unlock();
            qApp->processEvents();
            usleep(10000);
            qApp->lock();
        }
    }

    state = kStarting; // restart playback preview
    setEnabled(true);

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
}

void PlaybackBox::stop(ProgramInfo *rec)
{
    RemoteStopRecording(rec);
}

void PlaybackBox::doRemove(ProgramInfo *rec, bool forgetHistory)
{
    RemoteDeleteRecording(rec, forgetHistory);

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
    state = kStopping;

    if(delitem)
        delete delitem;

    delitem = new ProgramInfo(*toDel);
    showDeletePopup(delitem, DeleteRecording);
}

void PlaybackBox::expire(ProgramInfo *toExp)
{
    state = kStopping;

    if(delitem)
        delete delitem;
    delitem = new ProgramInfo(*toExp);

    showDeletePopup(delitem, AutoExpireRecording);
}

void PlaybackBox::showActions(ProgramInfo *toExp)
{
    //killPlayer();

    if(delitem)
        delete delitem;

    delitem = new ProgramInfo(*toExp);

    showActionPopup(delitem);
}

void PlaybackBox::showDeletePopup(ProgramInfo *program, deletePopupType types)
{
    updateFreeSpace = true;

    //enableGrayOut = true;

    stopPreview();

    popup = new MythPopupBox(gContext->GetMainWindow(), graphicPopup,
                             popupForeground, popupBackground,
                             popupHighlight, "delete popup");
    QString message1;
    switch (types)
    {
        case EndOfRecording:
             message1 = tr("You have finished watching:"); break;
        case DeleteRecording:
             message1 = tr("Are you sure you want to delete:"); break;
        case AutoExpireRecording:
             message1 = tr("Allow this program to AutoExpire?"); break;
        case StopRecording:
             message1 = tr("Are you sure you want to stop:"); break;
    }
    
    initPopup(popup, program, message1, "");

    QString tmpmessage;
    const char *tmpslot = NULL;

    if ((types == EndOfRecording || types == DeleteRecording) &&
        (program->IsSameProgram(*program)))
    {
        tmpmessage = tr("Yes, but allow future recordings of this episode"); 
        tmpslot = SLOT(doDeleteForgetHistory());
        popup->addButton(tmpmessage, this, tmpslot);
    }

    switch (types)
    {
        case EndOfRecording:
        case DeleteRecording:
             tmpmessage = tr("Yes, delete it"); 
             tmpslot = SLOT(doDelete());
             break;
        case AutoExpireRecording:
             tmpmessage = tr("Yes, AutoExpire"); 
             tmpslot = SLOT(doAutoExpire());
             break;
        case StopRecording:
             tmpmessage = tr("Yes, stop recording it"); 
             tmpslot = SLOT(doStop());
             break;
    }
    QButton *yesButton = popup->addButton(tmpmessage, this, tmpslot);

    switch (types)
    {
        case EndOfRecording:
             tmpmessage = tr("No, I might want to watch it again."); 
             tmpslot = SLOT(noDelete());
             break;
        case DeleteRecording:
             tmpmessage = tr("No, keep it, I changed my mind"); 
             tmpslot = SLOT(noDelete());
             break;
        case AutoExpireRecording:
             tmpmessage = tr("No, do not AutoExpire"); 
             tmpslot = SLOT(noAutoExpire());
             break;
        case StopRecording:
             tmpmessage = tr("No, continue recording it"); 
             tmpslot = SLOT(noStop());
             break;
    }
    QButton *noButton = popup->addButton(tmpmessage, this, tmpslot);

    if (types == EndOfRecording || types == DeleteRecording)
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
/*    backup.begin(this);
    grayOut(&backup);
    backup.end();*/
    //enableGrayOut = true;

stopPreview();

    popup = new MythPopupBox(gContext->GetMainWindow(), graphicPopup,
                             popupForeground, popupBackground,
                             popupHighlight, "action popup");

    initPopup(popup, program, "", tr("Select action:"));

    QSqlDatabase *db = QSqlDatabase::database();

    QButton *playButton = popup->addButton(tr("Play"), this, SLOT(doPlay()));


    if (RemoteGetRecordingStatus(program, overrectime, underrectime) > 0)
        popup->addButton(tr("Stop Recording"), this, SLOT(askStop()));

    if (delitem && delitem->GetAutoExpireFromRecorded(db))
        popup->addButton(tr("Don't Auto Expire"), this, SLOT(noAutoExpire()));
    else
        popup->addButton(tr("Auto Expire"), this, SLOT(doAutoExpire()));

    popup->addButton(tr("Change Recording Group"), this,
                     SLOT(showRecGroupChanger()));

    popup->addButton(tr("Edit Recording Schedule"), this,
                     SLOT(doEditScheduled()));

    QString query = QString("SELECT * FROM transcoding WHERE "
                            "chanid = '%1' AND starttime = '%2';")
                           .arg(curitem->chanid)
                           .arg(curitem->startts.toString("yyyyMMddhhmmss"));

    MythContext::KickDatabase(db);
    QSqlQuery result = db->exec(query);

    if (result.isActive() && result.numRowsAffected() > 0)
	popup->addButton(tr("Stop Transcoding"), this,
		     SLOT(doBeginTranscoding()));
    else
	popup->addButton(tr("Begin Transcoding"), this,
		     SLOT(doBeginTranscoding()));

    popup->addButton(tr("Delete"), this, SLOT(askDelete()));
    popup->addButton(tr("Cancel"), this, SLOT(doCancel()));

    popup->ShowPopup(this, SLOT(doCancel()));

    playButton->setFocus();

    expectingPopup = true;
}

void PlaybackBox::initPopup(MythPopupBox *popup, ProgramInfo *program,
                            QString message, QString message2)
{
    //killPlayerSafe();
    //stopPreview();
    //enableGrayOut = true;

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
        popup->addLabel(message);

    popup->addLabel(program->title, MythPopupBox::Large);

    if ((program->subtitle).stripWhiteSpace().length() > 0)
        popup->addLabel("\"" + program->subtitle + "\"\n" + timedate);
    else
        popup->addLabel(timedate);

    if (message2.stripWhiteSpace().length() > 0)
        popup->addLabel(message2);
}

//do we need a closePopup that doesn't restart the preview and stuff like that?
void PlaybackBox::cancelPopup(void)
{
    popup->hide();
    expectingPopup = false;
    enableGrayOut = false;

    //backup.begin(this);
    //backup.drawPixmap(0, 0, myBackground);
    //backup.end();

    delete popup;
    popup = NULL;

    timer->start(500);
    playingVideo = true;

    skipUpdate = false;
    skipCnt = 2;
    updateBackground();
    update(fullRect);
    setActiveWindow();
}

//stop? pause?
void PlaybackBox::stopPreview(void)
{
    //killPlayerSafe();
    timer->stop();
    playingVideo = false;
}


void PlaybackBox::doPlay(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    play(delitem);
}

void PlaybackBox::askStop(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    showDeletePopup(delitem, StopRecording);
}

void PlaybackBox::noStop(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    state = kChanging;

    timer->start(500);
}

void PlaybackBox::doStop(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    stop(delitem);

    state = kChanging;

    timer->start(500);
}

void PlaybackBox::doEditScheduled()
{
    if (!expectingPopup)
        return;

    cancelPopup();

    QSqlDatabase *db = QSqlDatabase::database();

    if (gContext->GetNumSetting("AdvancedRecord", 0))
    {
        ScheduledRecording record;
        record.loadByProgram(db, curitem);
        record.exec(db);
    }
    else
    {
        InfoDialog diag(curitem, gContext->GetMainWindow(), "Program Info");
        diag.exec();
    }
    
    ScheduledRecording::signalChange(db);

    connected = FillList();
    update(fullRect);
}    

void PlaybackBox::doBeginTranscoding()
{
    if (!expectingPopup)
        return;

    cancelPopup();

    QSqlDatabase *db = QSqlDatabase::database();

    QString query = QString("SELECT * FROM transcoding WHERE "
                            "chanid = '%1' AND starttime = '%2';")
                           .arg(curitem->chanid)
                           .arg(curitem->startts.toString("yyyyMMddhhmmss"));

    MythContext::KickDatabase(db);

    QSqlQuery result = db->exec(query);

    if(result.isActive() && result.numRowsAffected() > 0)
        RemoteQueueTranscode(curitem, TRANSCODE_STOP);
    else
        RemoteQueueTranscode(curitem, TRANSCODE_QUEUED |
                                      TRANSCODE_USE_CUTLIST);

}

void PlaybackBox::askDelete(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    showDeletePopup(delitem, DeleteRecording);
}

void PlaybackBox::noDelete(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    state = kChanging;

    timer->start(500);
}

void PlaybackBox::doDelete(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    doRemove(delitem, false);

    state = kChanging;

    timer->start(500);
}

void PlaybackBox::doDeleteForgetHistory(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    doRemove(delitem, true);

    state = kChanging;

    timer->start(500);
}

void PlaybackBox::noAutoExpire(void)
{
    if (!expectingPopup && delitem)
        return;

    cancelPopup();

    QSqlDatabase *db = QSqlDatabase::database();
    delitem->SetAutoExpire(false, db);

    state = kChanging;
}

void PlaybackBox::doAutoExpire(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    QSqlDatabase *db = QSqlDatabase::database();
    delitem->SetAutoExpire(true, db);

    state = kChanging;
}

void PlaybackBox::doCancel(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    state = kChanging;
}

void PlaybackBox::promptEndOfRecording(ProgramInfo *rec)
{
    if (!rec)
        return;

    state = kStopping;

    if (!rec)
        return;

    if(delitem)
        delete delitem;
        
    delitem = new ProgramInfo(*rec);
    showDeletePopup(delitem, EndOfRecording);
}

void PlaybackBox::UpdateProgressBar(void)
{
    update(usageRect);
}

void PlaybackBox::timeout(void)
{
    if (showData.count() == 0)
        return;

    update(videoRect);
}

void PlaybackBox::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "ESCAPE")
            exitWin();
        else if (action == "1" || action == "HELP")
            showIconHelp();
        else if (action == "MENU")
            showMenu();
        else if (showData.count() > 0)
        {
            if (action == "DELETE")
                deleteSelected();
            else if (action == "PLAYBACK")
                playSelected();
            else if (action == "INFO")
                showActionsSelected();
            else if (action == "SELECT")
                selected();
            else if (action == "UP")
                cursorUp();
            else if (action == "DOWN")
                cursorDown();
            else if (action == "LEFT")
                cursorLeft();
            else if (action == "RIGHT")
                cursorRight();
            else if (action == "PAGEUP")
                pageUp();
            else if (action == "PAGEDOWN")
                pageDown();
            else if (action == "PREVVIEW")
                cursorUp(false, true);
            else if (action == "NEXTVIEW")
                cursorDown(false, true);
            else
                handled = false;
        }
        else
            handled = false;
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void PlaybackBox::customEvent(QCustomEvent *e)
{
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
        bool ret = RemoteCheckFile(pginfo);
        return ret;
    }

    QFile checkFile(pginfo->pathname);

    return checkFile.exists();
}

QPixmap PlaybackBox::getPixmap(ProgramInfo *pginfo)
{
    QPixmap retpixmap;

    if(!generatePreviewPixmap)
        return retpixmap;
        
    // Check and see if we've already tried this one.    
    if(pginfo == previewProgamInfo)
    {
        if(previewPixmap)
            retpixmap = *previewPixmap;
        
        return retpixmap;
    }
    
    if(previewPixmap)
    {
        delete previewPixmap;
        previewPixmap = NULL;
    }
    
    QString filename = pginfo->pathname;
    filename += ".png";

    int screenheight = 0, screenwidth = 0;
    float wmult = 0, hmult = 0;

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    previewPixmap = gContext->LoadScalePixmap(filename);
    if (previewPixmap)
    {
        retpixmap = *previewPixmap;
        return retpixmap;
    }

    QImage *image = gContext->CacheRemotePixmap(filename);

    if (!image)
    {
        RemoteGeneratePreviewPixmap(pginfo);

        previewPixmap = gContext->LoadScalePixmap(filename);
        if (previewPixmap)
        {
            retpixmap = *previewPixmap;
            return retpixmap;
        }

        image = gContext->CacheRemotePixmap(filename);
    }

    if (image)
    {
        previewPixmap = new QPixmap();

        if (screenwidth != 800 || screenheight != 600)
        {
            QImage tmp2 = image->smoothScale((int)(image->width() * wmult),
                                             (int)(image->height() * hmult));
            previewPixmap->convertFromImage(tmp2);
        }
        else
        {
            previewPixmap->convertFromImage(*image);
        }
    }
 
    if (!previewPixmap)
    {
        previewPixmap = new QPixmap((int)(160 * wmult), (int)(120 * hmult));
        previewPixmap->fill(black);
    }

    retpixmap = *previewPixmap;
    return retpixmap;
}

void PlaybackBox::showIconHelp(void)
{
    LayerSet *container = NULL;
    if (type != Delete)
        container = theme->GetSet("program_info_play");
    else
        container = theme->GetSet("program_info_del");

    if (!container)
        return;

    MythPopupBox *iconhelp = new MythPopupBox(gContext->GetMainWindow(),
                                              true, popupForeground,
                                              popupBackground, popupHighlight,
                                              "icon help");

    QGridLayout *grid = new QGridLayout(5, 2, (int)(10 * wmult));

    QLabel *label;
    UIImageType *itype;
    bool displayme = false;

    label = iconhelp->addLabel(tr("Status Icons"), MythPopupBox::Large,
                               false);

    label->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    itype = (UIImageType *)container->GetType("commflagged");
    if (itype)
    {
        label = new QLabel(tr("Commercials are flagged"), iconhelp);
        label->setAlignment(Qt::WordBreak | Qt::AlignLeft);
        label->setBackgroundOrigin(ParentOrigin);
        label->setPaletteForegroundColor(popupForeground);
        grid->addWidget(label, 0, 1, Qt::AlignLeft);
         
        label = new QLabel(iconhelp, "nopopsize");

        itype->ResetFilename();
        itype->LoadImage();
        label->setPixmap(itype->GetImage());
        displayme = true;

        label->setMaximumWidth(width() / 2);
        label->setBackgroundOrigin(ParentOrigin);
        label->setPaletteForegroundColor(popupForeground);
        grid->addWidget(label, 0, 0, Qt::AlignCenter);
    }

    itype = (UIImageType *)container->GetType("cutlist");
    if (itype)
    {
        label = new QLabel(tr("An editing cutlist is present"), iconhelp);
        label->setAlignment(Qt::WordBreak | Qt::AlignLeft);
        label->setBackgroundOrigin(ParentOrigin);
        label->setPaletteForegroundColor(popupForeground);
        grid->addWidget(label, 1, 1, Qt::AlignLeft);

        label = new QLabel(iconhelp, "nopopsize");

        itype->ResetFilename();
        itype->LoadImage();
        label->setPixmap(itype->GetImage());
        displayme = true;

        label->setMaximumWidth(width() / 2);
        label->setBackgroundOrigin(ParentOrigin);
        label->setPaletteForegroundColor(popupForeground);
        grid->addWidget(label, 1, 0, Qt::AlignCenter);
    }

    itype = (UIImageType *)container->GetType("autoexpire");
    if (itype)
    {
        label = new QLabel(tr("The program is able to auto-expire"), iconhelp);
        label->setAlignment(Qt::WordBreak | Qt::AlignLeft);
        label->setBackgroundOrigin(ParentOrigin);
        label->setPaletteForegroundColor(popupForeground);
        grid->addWidget(label, 2, 1, Qt::AlignLeft);

        label = new QLabel(iconhelp, "nopopsize");

        itype->ResetFilename();
        itype->LoadImage();
        label->setPixmap(itype->GetImage());
        displayme = true;

        label->setMaximumWidth(width() / 2);
        label->setBackgroundOrigin(ParentOrigin);
        label->setPaletteForegroundColor(popupForeground);
        grid->addWidget(label, 2, 0, Qt::AlignCenter);
    }

    itype = (UIImageType *)container->GetType("processing");
    if (itype)
    {
        label = new QLabel(tr("Commercials are being flagged"), iconhelp);
        label->setAlignment(Qt::WordBreak | Qt::AlignLeft);
        label->setBackgroundOrigin(ParentOrigin);
        label->setPaletteForegroundColor(popupForeground);
        grid->addWidget(label, 3, 1, Qt::AlignLeft);

        label = new QLabel(iconhelp, "nopopsize");

        itype->ResetFilename();
        itype->LoadImage();
        label->setPixmap(itype->GetImage());
        displayme = true;

        label->setMaximumWidth(width() / 2);
        label->setBackgroundOrigin(ParentOrigin);
        label->setPaletteForegroundColor(popupForeground);
        grid->addWidget(label, 3, 0, Qt::AlignCenter);
    }

    itype = (UIImageType *)container->GetType("bookmark");
    if (itype)
    {
        label = new QLabel(tr("A bookmark is set"), iconhelp);
        label->setAlignment(Qt::WordBreak | Qt::AlignLeft);
        label->setBackgroundOrigin(ParentOrigin);
        label->setPaletteForegroundColor(popupForeground);
        grid->addWidget(label, 4, 1, Qt::AlignLeft);

        label = new QLabel(iconhelp, "nopopsize");

        itype->ResetFilename();
        itype->LoadImage();
        label->setPixmap(itype->GetImage());
        displayme = true;

        label->setMaximumWidth(width() / 2);
        label->setBackgroundOrigin(ParentOrigin);
        label->setPaletteForegroundColor(popupForeground);
        grid->addWidget(label, 4, 0, Qt::AlignCenter);
    }

    if (!displayme)
    {
        delete iconhelp;
        return;
    }

    //killPlayerSafe();

    //backup.begin(this);
    //grayOut(&backup);
    //backup.end();

    stopPreview();

    enableGrayOut = true;

    iconhelp->addLayout(grid);

    QButton *button = iconhelp->addButton(tr("Ok"));
    button->setFocus();

    iconhelp->ExecPopup();

    delete iconhelp;

    //backup.begin(this);
    //backup.drawPixmap(0, 0, myBackground);
    //backup.end();
    enableGrayOut = false;

    state = kChanging;

    skipUpdate = false;
    skipCnt = 2;
    update(fullRect);

    setActiveWindow();
}

void PlaybackBox::showRecGroupChooser(void)
{
    if (expectingPopup)
        cancelPopup();

    MythPopupBox tmpPopup(gContext->GetMainWindow(),
                                              true, popupForeground,
                                              popupBackground, popupHighlight,
                                              "choose recgroup view");
    choosePopup = &tmpPopup;

    //timer->stop();
    //playingVideo = false;

    stopPreview();
    enableGrayOut = true;

    //backup.begin(this);
    //grayOut(&backup);
    //backup.end();

    QLabel *label = choosePopup->addLabel(tr("Recording Group View"),
                                  MythPopupBox::Large, false);
    label->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    QStringList groups;
    QSqlDatabase *m_db = QSqlDatabase::database();
    QString thequery = QString("SELECT DISTINCT recgroup from recorded");
    QSqlQuery query = m_db->exec(thequery);

    QString tmpType = recGroupType[recGroup];
    recGroupType.clear();

    recGroupType[recGroup] = tmpType;
    groups += recGroup;
    if (query.isActive() && query.numRowsAffected() > 0)
        while (query.next())
            if (query.value(0).toString() != recGroup)
            {
                QString key = query.value(0).toString();
                groups += key;
                recGroupType[key] = "recgroup";
            }

    if (gContext->GetNumSetting("UseCategoriesAsRecGroups"))
    {
        thequery = QString("SELECT DISTINCT category from recorded");
        query = m_db->exec(thequery);

        if (query.isActive() && query.numRowsAffected() > 0)
            while (query.next())
                if ((query.value(0).toString() != recGroup) &&
                    (query.value(0).toString() != ""))
                {
                    QString key = query.value(0).toString();
                    groups += key;
                    if ( !recGroupType.contains(key))
                        recGroupType[key] = "category";
                }
    }

    if (recGroup != tr("All Programs"))
    {
        groups += tr("All Programs");
        recGroupType[tr("All Programs")] = "recgroup";
    }

    QGridLayout *grid = new QGridLayout(1, 2, (int)(10 * wmult));

    label = new QLabel("Group", choosePopup);
    label->setAlignment(Qt::WordBreak | Qt::AlignLeft);
    label->setBackgroundOrigin(ParentOrigin);
    label->setPaletteForegroundColor(popupForeground);
    grid->addWidget(label, 0, 0, Qt::AlignLeft);

    chooseComboBox = new MythComboBox(false, choosePopup);
    chooseComboBox->insertStringList(groups);
    chooseComboBox->setAcceptOnSelect(true);
    grid->addWidget(chooseComboBox, 0, 1, Qt::AlignLeft);

    choosePopup->addLayout(grid);

    connect(chooseComboBox, SIGNAL(accepted(int)), this,
            SLOT(chooseSetViewGroup()));
    connect(chooseComboBox, SIGNAL(activated(int)), this,
            SLOT(chooseComboBoxChanged()));
    connect(chooseComboBox, SIGNAL(highlighted(int)), this,
            SLOT(chooseComboBoxChanged()));

    chooseGroupPassword = getRecGroupPassword(recGroup);

    chooseComboBox->setFocus();
    choosePopup->ExecPopup();

    delete chooseComboBox;
    chooseComboBox = NULL;

    //backup.begin(this);
    //backup.drawPixmap(0, 0, myBackground);
    //backup.end();

    enableGrayOut = false;

    skipUpdate = false;
    skipCnt = 2;
    update(fullRect);

    setActiveWindow();

    timer->start(500);
}

void PlaybackBox::chooseSetViewGroup(void)
{
    if (!chooseComboBox)
        return;

    recGroup = chooseComboBox->currentText();
    recGroupPassword = chooseGroupPassword;

    if(groupnameAsAllProg)
        groupDisplayName = recGroup;

    if (recGroupPassword != "" )
    {
        bool ok = false;
        QString text = "Password:";

        MythPasswordDialog *pwd = new MythPasswordDialog(text, &ok,
                                                     recGroupPassword,
                                                     gContext->GetMainWindow());
        pwd->exec();
        delete pwd;
        if (!ok)
            return;

        curGroupPassword = recGroupPassword;
    } else
        curGroupPassword = "";

    chooseGroupPassword = "";

    if (gContext->GetNumSetting("RememberRecGroup",1))
        gContext->SaveSetting("DisplayRecGroup", recGroup);

    if (recGroupType[recGroup] == "recgroup")
        gContext->SaveSetting("DisplayRecGroupIsCategory", 0);
    else
        gContext->SaveSetting("DisplayRecGroupIsCategory", 1);

    inTitle = true;
    curTitle = 0;
    curShowing = 0;
    skipNum = 0;

    connected = FillList();
    skipUpdate = false;
    update(fullRect);
    choosePopup->done(0);
}

void PlaybackBox::chooseComboBoxChanged(void)
{
    if (!chooseComboBox)
        return;

    QString newGroup = chooseComboBox->currentText();

    chooseGroupPassword = getRecGroupPassword(newGroup);
}

QString PlaybackBox::getRecGroupPassword(QString group)
{
    QString result = QString("");

    if (group == tr("All Programs"))
    {
        result = gContext->GetSetting("AllRecGroupPassword");
    }
    else
    {
        QSqlDatabase *m_db = QSqlDatabase::database();
        QString thequery = QString("SELECT password FROM recgrouppassword "
                                   "WHERE recgroup = '%1';").arg(group);
        QSqlQuery query = m_db->exec(thequery);

        if (query.isActive() && query.numRowsAffected() > 0)
            if (query.next())
                result = query.value(0).toString();
    }

    if (result == QString::null)
        result = QString("");

    return(result);
}

void PlaybackBox::fillRecGroupPasswordCache(void)
{
    recGroupPwCache.clear();

    QSqlDatabase *m_db = QSqlDatabase::database();
    QString thequery = QString("SELECT recgroup, password "
                               "FROM recgrouppassword "
                               "WHERE password is not null "
                               "AND password <> '' ");
    QSqlQuery query = m_db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
        while (query.next())
            recGroupPwCache[query.value(0).toString()] =
                query.value(1).toString();
}

void PlaybackBox::showRecGroupChanger(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    enableGrayOut = true;

    MythPopupBox tmpPopup(gContext->GetMainWindow(),
                                              true, popupForeground,
                                              popupBackground, popupHighlight,
                                              "change recgroup view");
    choosePopup = &tmpPopup;


    QLabel *label = choosePopup->addLabel(tr("Change Recording Group"),
                                  MythPopupBox::Large, false);
    label->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    QStringList groups;
    QSqlDatabase *m_db = QSqlDatabase::database();
    QString thequery = QString("SELECT DISTINCT recgroup from recorded");
    QSqlQuery query = m_db->exec(thequery);

    groups += delitem->recgroup;
    if (query.isActive() && query.numRowsAffected() > 0)
        while (query.next())
            if (query.value(0).toString() != delitem->recgroup)
                groups += query.value(0).toString();

    chooseComboBox = new MythComboBox(false, choosePopup);
    chooseComboBox->insertStringList(groups);
    choosePopup->addWidget(chooseComboBox);

    chooseLineEdit = new MythLineEdit(choosePopup);
    chooseLineEdit->setText("");
    chooseLineEdit->selectAll();
    choosePopup->addWidget(chooseLineEdit);

    chooseOkButton = new MythPushButton(choosePopup);
    chooseOkButton->setText(tr("OK"));
    chooseOkButton->setEnabled(true);
    choosePopup->addWidget(chooseOkButton);

    connect(chooseOkButton, SIGNAL(clicked()), this,
            SLOT(changeSetRecGroup()));
    connect(chooseOkButton, SIGNAL(clicked()), choosePopup, SLOT(accept()));
    connect(chooseComboBox, SIGNAL(activated(int)), this,
            SLOT(changeComboBoxChanged()));
    connect(chooseComboBox, SIGNAL(highlighted(int)), this,
            SLOT(changeComboBoxChanged()));

    chooseLineEdit->setText(delitem->recgroup);

    chooseComboBox->setFocus();
    choosePopup->ExecPopup();

    delete chooseLineEdit;
    chooseLineEdit = NULL;
    delete chooseOkButton;
    chooseOkButton = NULL;
    delete chooseComboBox;
    chooseComboBox = NULL;

    if(delitem)
        delete delitem;
    delitem = NULL;
}

void PlaybackBox::changeSetRecGroup(void)
{
    if (!chooseComboBox || !chooseLineEdit)
        return;

    QString newRecGroup = chooseLineEdit->text();

    if (newRecGroup == "" ) 
        return;

    QSqlDatabase *m_db = QSqlDatabase::database();
    delitem->ApplyRecordRecGroupChange(m_db, newRecGroup);
    
    inTitle = true;
    curTitle = 0;
    curShowing = 0;
    skipNum = 0;

    connected = FillList();
    skipUpdate = false;
    update(fullRect);
}

void PlaybackBox::changeComboBoxChanged(void)
{
    if (!chooseComboBox || !chooseLineEdit)
        return;

    QString newGroup = chooseComboBox->currentText();

    chooseLineEdit->setText(newGroup);
}

void PlaybackBox::showRecGroupPasswordChanger(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    MythPopupBox tmpPopup(gContext->GetMainWindow(),
                                              true, popupForeground,
                                              popupBackground, popupHighlight,
                                              "change recgroup password");
    choosePopup = &tmpPopup;

    stopPreview();
    enableGrayOut = true;

    //timer->stop();
    //playingVideo = false;

    //backup.begin(this);
    //grayOut(&backup);
    //backup.end();

    QLabel *label = choosePopup->addLabel(tr("Change Recording Group Password"),
                                  MythPopupBox::Large, false);
    label->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    QGridLayout *grid = new QGridLayout(3, 2, (int)(10 * wmult));

    label = new QLabel(tr("Recording Group:"), choosePopup);
    label->setAlignment(Qt::WordBreak | Qt::AlignLeft);
    label->setBackgroundOrigin(ParentOrigin);
    label->setPaletteForegroundColor(popupForeground);
    grid->addWidget(label, 0, 0, Qt::AlignLeft);

    label = new QLabel(recGroup, choosePopup);
    label->setAlignment(Qt::WordBreak | Qt::AlignLeft);
    label->setBackgroundOrigin(ParentOrigin);
    label->setPaletteForegroundColor(popupForeground);
    grid->addWidget(label, 0, 1, Qt::AlignLeft);

    label = new QLabel(tr("Old Password:"), choosePopup);
    label->setAlignment(Qt::WordBreak | Qt::AlignLeft);
    label->setBackgroundOrigin(ParentOrigin);
    label->setPaletteForegroundColor(popupForeground);
    grid->addWidget(label, 1, 0, Qt::AlignLeft);

    chooseOldPassword = new MythLineEdit(choosePopup);
    chooseOldPassword->setText("");
    chooseOldPassword->selectAll();
    grid->addWidget(chooseOldPassword, 1, 1, Qt::AlignLeft);

    label = new QLabel(tr("New Password:"), choosePopup);
    label->setAlignment(Qt::WordBreak | Qt::AlignLeft);
    label->setBackgroundOrigin(ParentOrigin);
    label->setPaletteForegroundColor(popupForeground);
    grid->addWidget(label, 2, 0, Qt::AlignLeft);

    chooseNewPassword = new MythLineEdit(choosePopup);
    chooseNewPassword->setText("");
    chooseNewPassword->selectAll();
    grid->addWidget(chooseNewPassword, 2, 1, Qt::AlignLeft);

    choosePopup->addLayout(grid);

    chooseOkButton = new MythPushButton(choosePopup);
    chooseOkButton->setText(tr("OK"));
    choosePopup->addWidget(chooseOkButton);

    connect(chooseOldPassword, SIGNAL(textChanged(const QString &)), this,
            SLOT(changeOldPasswordChanged(const QString &)));
    connect(chooseOkButton, SIGNAL(clicked()), this,
            SLOT(changeRecGroupPassword()));
    connect(chooseOkButton, SIGNAL(clicked()), choosePopup, SLOT(accept()));

    chooseGroupPassword = getRecGroupPassword(recGroup);

    chooseOldPassword->setEchoMode(QLineEdit::Password);
    chooseNewPassword->setEchoMode(QLineEdit::Password);

    if (recGroupPassword == "" )
        chooseOkButton->setEnabled(true);
    else
        chooseOkButton->setEnabled(false);

    chooseOldPassword->setFocus();
    choosePopup->ExecPopup();

    delete chooseOldPassword;
    chooseOldPassword = NULL;
    delete chooseNewPassword;
    chooseNewPassword = NULL;
    delete chooseOkButton;
    chooseOkButton = NULL;

    //backup.begin(this);
    //backup.drawPixmap(0, 0, myBackground);
    //backup.end();
    enableGrayOut = false;

    skipUpdate = false;
    skipCnt = 2;
    update(fullRect);

    setActiveWindow();

    timer->start(500);
}

void PlaybackBox::changeRecGroupPassword(void)
{
    QString newPassword = chooseNewPassword->text();

    if (chooseOldPassword->text() != recGroupPassword)
        return;

    if (recGroup == tr("All Programs"))
    {
        gContext->SaveSetting("AllRecGroupPassword", newPassword);
    }
    else
    {
        QSqlDatabase *m_db = QSqlDatabase::database();
        QString thequery;

        thequery = QString("DELETE FROM recgrouppassword "
                           "WHERE recgroup = '%1'").arg(recGroup);
        m_db->exec(thequery);

        if (newPassword != "")
        {
            thequery = QString("INSERT INTO recgrouppassword "
                               "(recgroup, password) VALUES "
                               "('%1', '%2')").arg(recGroup).arg(newPassword);
            m_db->exec(thequery);
        }
    }
}

void PlaybackBox::changeOldPasswordChanged(const QString &newText)
{
    if (newText == recGroupPassword)
        chooseOkButton->setEnabled(true);
    else
        chooseOkButton->setEnabled(false);
}
