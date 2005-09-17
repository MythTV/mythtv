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
#include <qfileinfo.h>
#include <qsqldatabase.h>
#include <qmap.h>

#include <unistd.h>

#include <iostream>
using namespace std;

#include "playbackbox.h"
#include "proglist.h"
#include "tv.h"
#include "oldsettings.h"
#include "NuppelVideoPlayer.h"

#include "exitcodes.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "programinfo.h"
#include "scheduledrecording.h"
#include "remoteutil.h"
#include "jobqueue.h"
#include "lcddevice.h"

static int comp_programid(ProgramInfo *a, ProgramInfo *b)
{
    if (a->programid == b->programid)
    {
        if (a->recstartts == b->recstartts)
            return 0;
        if (a->recstartts < b->recstartts)
            return -1;
        else
            return 1;
    }
    if (a->programid < b->programid)
        return -1;
    else
        return 1;
}

static int comp_originalAirDate(ProgramInfo *a, ProgramInfo *b)
{
    QDate dt1, dt2;

    if (a->hasAirDate)
        dt1 = a->originalAirDate;
    else
        dt1 = a->startts.date();

    if (b->hasAirDate)
        dt2 = b->originalAirDate;
    else
        dt2 = b->startts.date();

    if (dt1 == dt2)
    {
        if (a->recstartts == b->recstartts)
            return 0;
        if (a->recstartts < b->recstartts)
            return -1;
        else
            return 1;
    }
    if (dt1 < dt2)
        return -1;
    else
        return 1;
}

PlaybackBox::PlaybackBox(BoxType ltype, MythMainWindow *parent, 
                         const char *name)
           : MythDialog(parent, name)
{
    type = ltype;

    everStartedVideo = false;    
    state = kStopped;
    killState = kDone;
    waitToStart = false;

    connected = false;
    rbuffer = NULL;
    nvp = NULL;

    playingSomething = false;

    inTitle = gContext->GetNumSetting("PlaybackBoxStartInTitle", 0);

    progLists[""];
    titleList << "";
    progIndex = 0;
    titleIndex = 0;
    playList.clear();
    onPlaylist = false;

    skipUpdate = false;
    skipCnt = 0;
    
    previewPixmap = NULL;
    previewStartts = QDateTime::currentDateTime();
    previewChanid = "";
    
    updateFreeSpace = true;
    freeSpaceTotal = 0;
    freeSpaceUsed = 0;

    leftRight = false;      // If change is left or right, don't restart video
    playingVideo = false;
    graphicPopup = true;
    expectingPopup = false;

    popup = NULL;
    curitem = NULL;
    delitem = NULL;
    lastProgram = NULL;

    // titleView controls showing titles in group list 
    titleView = true;

    // useCategories controls showing categories in group list
    useCategories = false;

    // useRecGroups controls showing of recording groups in group list
    useRecGroups = false;

    if (gContext->GetNumSetting("UseArrowAccels", 1))
        arrowAccel = true;
    else
        arrowAccel = false;

    groupnameAsAllProg = gContext->GetNumSetting("DispRecGroupAsAllProg", 0);
    listOrder = gContext->GetNumSetting("PlayBoxOrdering", 1);    
    recGroupType.clear();

    curGroupPassword = QString("");
    recGroup = gContext->GetSetting("DisplayRecGroup", QString("All Programs"));
    VERBOSE( VB_GENERAL, recGroup);
    if (groupnameAsAllProg)
    {
        if ((recGroup == "Default") || (recGroup == "All Programs"))
            groupDisplayName = tr(recGroup);
        else
            groupDisplayName = recGroup;
    }
    else
        groupDisplayName = tr("All Programs");

    recGroupPassword = getRecGroupPassword(recGroup);

    if (gContext->GetNumSetting("DisplayRecGroupIsCategory",0))
        recGroupType[recGroup] = "category";
    else
        recGroupType[recGroup] = "recgroup";

    setDefaultView(gContext->GetNumSetting("DisplayGroupDefaultView", 0));

    fullRect = QRect(0, 0, size().width(), size().height());
    listRect = QRect(0, 0, 0, 0);
    infoRect = QRect(0, 0, 0, 0);
    groupRect = QRect(0, 0, 0, 0);
    usageRect = QRect(0, 0, 0, 0);
    videoRect = QRect(0, 0, 0, 0);
    curGroupRect = QRect(0, 0, 0, 0);

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
    UIListType *listtype = NULL;
    if (container)
    {
        listtype = (UIListType *)container->GetType("showing");
        if (listtype)
            listsize = listtype->GetItems();
    }
    else
    {
        MythPopupBox::showOkPopup(
            gContext->GetMainWindow(),
            QObject::tr("Failed to get selector object"),
            QObject::tr(
                "Myth could not locate the selector object within your theme.\n"
                "Please make sure that your ui.xml is valid.\n"
                "\n"
                "Myth will now exit."));
                                  
        VERBOSE(VB_IMPORTANT, "Failed to get selector object.");
        exit(FRONTEND_BUGGY_EXIT_NO_SELECTOR);
        return;
    }

    if (theme->GetSet("group_info") && 
        gContext->GetNumSetting("ShowGroupInfo", 0) == 1)
    {
        haveGroupInfoSet = true;
        if (listtype)
            listtype->ShowSelAlways(false);
    }
    else
        haveGroupInfoSet = false;

    
    connected = FillList();

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

    if ((recGroupPassword != "") ||
        (gContext->GetNumSetting("QueryInitialFilter", 0)))
        showRecGroupChooser();

    // Initialize yuv2rgba conversion stuff
    conv_yuv2rgba  = yuv2rgb_init_mmx(32, MODE_RGB);
    conv_rgba_buf  = NULL;
    conv_rgba_size = QSize(0,0);
}

PlaybackBox::~PlaybackBox(void)
{
    gContext->removeListener(this);
    killPlayerSafe();
    delete timer;
    delete theme;
    delete bgTransBackup;
    
    if (previewPixmap)
        delete previewPixmap;
        
    if (curitem)
        delete curitem;
    if (delitem)
        delete delitem;

    if (conv_rgba_buf)
        delete [] conv_rgba_buf;

    if (lastProgram)
        delete lastProgram;
}

void PlaybackBox::setDefaultView(int defaultView)
{
    switch (defaultView)
    {
        default:
        case TitlesOnly: titleView = true; useCategories = false; 
                         useRecGroups = false; break;
        case TitlesCategories: titleView = true; useCategories = true; 
                               useRecGroups = false; break;
        case TitlesCategoriesRecGroups: titleView = true; useCategories = true;
                                        useRecGroups = true; break;
        case TitlesRecGroups: titleView = true; useCategories = false; 
                              useRecGroups = true; break;
        case Categories: titleView = false; useCategories = true; 
                         useRecGroups = false; break;
        case CategoriesRecGroups: titleView = false; useCategories = true; 
                                  useRecGroups = true; break;
        case RecGroups: titleView = false; useCategories = false; 
                        useRecGroups = true; break;
    }
}

/* blocks until playing has stopped */
void PlaybackBox::killPlayerSafe(void)
{
    /* don't process any keys while we are trying to let the nvp stop */
    /* if the user keeps selecting new recordings we will never stop playing */
    setEnabled(false);

    if (state != kKilled && state != kStopped && playbackPreview != 0 &&
        everStartedVideo)
    {
        while (state != kKilled)
        {
            /* ensure that key events don't mess up our states */
            if ((state != kKilling) && (state != kKilled))
                state = kKilling;

            /* NOTE: need unlock/process/lock here because we need
               to allow updateVideo() to run to handle changes in states */
            qApp->unlock();
            qApp->processEvents();
            usleep(500);
            qApp->lock();
        }
        state = kStopped;
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
                VERBOSE(VB_IMPORTANT,
                        QString("PlaybackBox::LoadWindow(): Ignoring unknown "
                                "element, %1. ").arg(e.tagName()));
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
    if (name.lower() == "group_info")
        groupRect = area;        
    if (name.lower() == "usage")
        usageRect = area;
    if (name.lower() == "cur_group")
        curGroupRect = area;        
    
}

void PlaybackBox::parsePopup(QDomElement &element)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty() || name != "confirmdelete")
    {
        if (name.isNull())
            name = "(null)";
        VERBOSE(VB_IMPORTANT,
                QString("PlaybackBox::parsePopup(): Popup name must "
                        "be 'confirmdelete' but was '%1'").arg(name));
        return;
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
                VERBOSE(VB_IMPORTANT,
                        QString("PlaybackBox::parsePopup(): Unknown child %1")
                        .arg(info.tagName()));
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
    
    if (r.intersects(curGroupRect) && skipUpdate == false)
    {
        updateCurGroup(&p);
    }
    
    if (r.intersects(usageRect) && skipUpdate == false)
    {
        updateUsage(&p);
    }
    
    if (r.intersects(videoRect))
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

void PlaybackBox::grayOut(QPainter *tmp)
{
    (void)tmp;
/*
    int transparentFlag = gContext->GetNumSetting("PlayBoxShading", 0);
    if (transparentFlag == 0)
        tmp->fillRect(QRect(QPoint(0, 0), size()), 
                      QBrush(QColor(10, 10, 10), Dense4Pattern));
    else if (transparentFlag == 1)
        tmp->drawPixmap(0, 0, *bgTransBackup, 0, 0, (int)(800*wmult), 
                        (int)(600*hmult));
*/
}
void PlaybackBox::updateCurGroup(QPainter *p)
{
    QRect pr = curGroupRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    if( recGroup != "Default")
        updateGroupInfo(p, pr, pix, "cur_group");
    else
    {
        LayerSet *container = theme->GetSet("cur_group");
        if (container)
        {
            QPainter tmp(&pix);
            
            container->ClearAllText();
            container->Draw(&tmp, 6, 1);

            tmp.end();
            p->drawPixmap(pr.topLeft(), pix);
        }
    
    }
}


void PlaybackBox::updateGroupInfo(QPainter *p, QRect& pr, QPixmap& pix, QString cont_name)
{
    LayerSet *container = theme->GetSet(cont_name);
    if( container)
    {    
        container->ClearAllText();
        QPainter tmp(&pix);
        QMap<QString, QString> infoMap;
        int countInGroup; // = progLists[""].count();
        
       
        if (titleList[titleIndex] == "")
        {
           countInGroup = progLists[""].count(); 
           infoMap["title"] = groupDisplayName;
           infoMap["group"] = groupDisplayName;
           infoMap["show"] = QObject::tr("All Programs");
        }
        else
        {                  
            countInGroup = progLists[titleList[titleIndex]].count();
            infoMap["group"] = groupDisplayName;
            infoMap["show"] = titleList[titleIndex];
            infoMap["title"] = QString("%1 - %2").arg(groupDisplayName)
                                                 .arg(titleList[titleIndex]);
        }
        
        if (countInGroup > 1)
            infoMap["description"] = QString(tr("There are %1 recordings in "
                                                "this display group"))
                                                .arg(countInGroup);
        else if (countInGroup == 1)
            infoMap["description"] = QString(tr("There is one recording in "
                                                 "this display group"));
        else
            infoMap["description"] = QString(tr("There are no recordings in "
                                                "this display group"));
        
        infoMap["rec_count"] = QString("%1").arg(countInGroup);
                                                
        container->SetText(infoMap);

        if (type != Delete)
            container->Draw(&tmp, 6, 0);
        else
            container->Draw(&tmp, 6, 1);

        tmp.end();
        p->drawPixmap(pr.topLeft(), pix);
    }
    else
    {
        if (cont_name == "group_info")
            updateProgramInfo(p, pr, pix);
    }
}


void PlaybackBox::updateProgramInfo(QPainter *p, QRect& pr, QPixmap& pix)
{
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
        if (curitem)
            curitem->ToMap(infoMap);
        
        if ((playbackPreview == 0) &&
            (generatePreviewPixmap == 0))
            container->UseAlternateArea(true);

        container->ClearAllText();
        container->SetText(infoMap);

        int flags = 0;
        if (curitem)
            flags = curitem->programflags;

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
    bool updateGroup = (inTitle && haveGroupInfoSet);
    if (updateGroup)
        pr = groupRect;
        
    QPixmap pix(pr.size());
    
    pix.fill(this, pr.topLeft());
    
    if (titleList.count() > 1 && curitem && !updateGroup)
        updateProgramInfo(p, pr, pix);
    else if (updateGroup)
        updateGroupInfo(p, pr, pix);
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
        if (!playingSomething)
        {
            QPixmap temp = getPixmap(curitem);
            if (temp.width() > 0)
            {
                p->drawPixmap(videoRect.x(), videoRect.y(), temp);
            }
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
            VERBOSE(VB_IMPORTANT, QString("Error: File '%1' missing.")
                    .arg(rec->pathname));

            state = kStopping;

            ProgramInfo *tmpItem = findMatchingProg(rec);
            if (tmpItem)
                tmpItem->availableStatus = asFileNotFound;

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

        if (nvp)
        {
            if (nvp->IsPlaying())
                state = kPlaying;
        }
        else
        {
            // already dead, so clean up
            killPlayer();
            return;
        }
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
    if ((state == kPlaying) && nvp->IsPlaying() && !playingSomething)
    {
        int w = 0, h = 0;
        VideoFrame *frame = nvp->GetCurrentFrame(w, h);

        if (w == 0 || h == 0 || !frame || !(frame->buf))
        {
            nvp->ReleaseCurrentFrame(frame);
            return;
        }

        unsigned char *yuv_buf = frame->buf;
        if (conv_rgba_size.width() != w || conv_rgba_size.height() != h)
        { 
            if (conv_rgba_buf)
                delete [] conv_rgba_buf;
            conv_rgba_buf = new unsigned char[w * h * 4];
        }

        conv_yuv2rgba(conv_rgba_buf,
                      yuv_buf, yuv_buf + (w * h), yuv_buf + (w * h * 5 / 4),
                      w, h, w * 4, w, w / 2, 0);

        nvp->ReleaseCurrentFrame(frame);

        QImage img(conv_rgba_buf, w, h, 32, NULL, 65536 * 65536, 
                   QImage::LittleEndian);
        img = img.scale(videoRect.width(), videoRect.height());

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

        vector<FileSystemInfo> fsInfos;
        if (updateFreeSpace && connected)
        {
            updateFreeSpace = false;
            fsInfos = RemoteGetFreeSpace();
            if (fsInfos.size()>0)
            {
                freeSpaceTotal = (int) (fsInfos[0].totalSpaceKB >> 10);
                freeSpaceUsed = (int) (fsInfos[0].usedSpaceKB >> 10);
            }
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
    QString tempTitle;
    QString tempSubTitle;
    QString tempDate;
    QString tempTime;
    QString tempSize;

    QString match;
    QRect pr = listRect;
    QPixmap pix(pr.size());

    LayerSet *container = NULL;
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    LCD *lcddev = LCD::Get();
    QString tstring, lcdTitle;
    QPtrList<LCDMenuItem> lcdItems;
    lcdItems.setAutoDelete(true);

    container = theme->GetSet("selector");

    ProgramInfo *tempInfo;

    ProgramList *plist;
    if (titleList.count() > 1)
        plist = &progLists[titleList[titleIndex]];
    else
        plist = &progLists[""];

    int progCount = plist->count();

    if (curitem)
    {
        delete curitem;
        curitem = NULL;
    }

    if (container && titleList.count() >= 1)
    {
        UIListType *ltype = (UIListType *)container->GetType("toptitles");
        if (ltype && titleList.count() > 1)
        {
            ltype->ResetList();
            ltype->SetActive(inTitle);

            int h = titleIndex - ltype->GetItems() +
                ltype->GetItems() * titleList.count();
            h = h % titleList.count();

            for (int cnt = 0; cnt < ltype->GetItems(); cnt++)
            {
                if (titleList[h] == "")
                    tstring = groupDisplayName;
                else
                    tstring = titleList[h];

                ltype->SetItemText(cnt, tstring);
                if (lcddev && inTitle)
                    lcdItems.append(new LCDMenuItem(0, NOTCHECKABLE, tstring));

                h++;
                h = h % titleList.count();
             }
        }
        else if (ltype)
        {
            ltype->ResetList();
        }

        UITextType *typeText = (UITextType *)container->GetType("current");
        if (typeText && titleList.count() > 1)
        {
            if (titleList[titleIndex] == "")
                tstring = groupDisplayName;
            else
                tstring = titleList[titleIndex];

            typeText->SetText(tstring);
            if (lcddev) 
            {
                if (inTitle)
                {
                    lcdItems.append(new LCDMenuItem(1, NOTCHECKABLE, tstring));
                    lcdTitle = "Recordings";
                }
                else
                    lcdTitle = " <<" + tstring;
            }
        }
        else if (typeText)
        {
            typeText->SetText("");
        }

        ltype = (UIListType *)container->GetType("bottomtitles");
        if (ltype && titleList.count() > 1)
        {
            ltype->ResetList();
            ltype->SetActive(inTitle);

            int h = titleIndex + 1;
            h = h % titleList.count();

            for (int cnt = 0; cnt < ltype->GetItems(); cnt++)
            {
                if (titleList[h] == "")
                    tstring = groupDisplayName;
                else
                    tstring = titleList[h];

                ltype->SetItemText(cnt, tstring);
                if (lcddev && inTitle)
                    lcdItems.append(new LCDMenuItem(0, NOTCHECKABLE, tstring));

                h++;
                h = h % titleList.count();
            }
        }
        else if (ltype)
        {
            ltype->ResetList();
        }

        ltype = (UIListType *)container->GetType("showing");
        if (ltype && titleList.count() > 1)
        {
            ltype->ResetList();
            ltype->SetActive(!inTitle);

            int skip;
            if (progCount <= listsize || progIndex <= listsize / 2)
                skip = 0;
            else if (progIndex >= progCount - listsize + listsize / 2)
                skip = progCount - listsize;
            else
                skip = progIndex - listsize / 2;

            ltype->SetUpArrow(skip > 0);
            ltype->SetDownArrow(skip + listsize < progCount);

            int cnt;
            for (cnt = 0; cnt < listsize; cnt++)
            {
                if (cnt + skip >= progCount)
                    break;

                tempInfo = plist->at(skip+cnt);

                if ((titleList[titleIndex] == "") || (!(titleView)))
                    tempSubTitle = tempInfo->title; 
                else
                    tempSubTitle = tempInfo->subtitle;
                if (tempSubTitle.stripWhiteSpace().length() == 0)
                    tempSubTitle = tempInfo->title;
                if ((tempInfo->subtitle).stripWhiteSpace().length() > 0 
                    && ((titleList[titleIndex] == "") || (!(titleView))))
                {
                    tempSubTitle = tempSubTitle + " - \"" + 
                        tempInfo->subtitle + "\"";
                }

                tempDate = (tempInfo->recstartts).toString(showDateFormat);
                tempTime = (tempInfo->recstartts).toString(showTimeFormat);

                long long size = tempInfo->filesize;
                tempSize.sprintf("%0.2f GB", size / 1024.0 / 1024.0 / 1024.0);

                if (skip + cnt == progIndex)
                {
                    curitem = new ProgramInfo(*tempInfo);
                    ltype->SetItemCurrent(cnt);
                }

                if (lcddev && !inTitle) 
                {
                    QString lcdSubTitle = tempSubTitle;
                    lcdItems.append(new LCDMenuItem(skip + cnt == progIndex,
                                    NOTCHECKABLE,
                                    lcdSubTitle.replace('"', "'") +
                                    " " + tempDate));                       
                }

                ltype->SetItemText(cnt, 1, tempSubTitle);
                ltype->SetItemText(cnt, 2, tempDate);
                ltype->SetItemText(cnt, 3, tempTime);
                ltype->SetItemText(cnt, 4, tempSize);
                if (tempInfo->recstatus == rsRecording)
                    ltype->EnableForcedFont(cnt, "recording");

                QString key;
                key = tempInfo->chanid + "_" +
                      tempInfo->recstartts.toString(Qt::ISODate);

                if (playList.grep(key).count())
                    ltype->EnableForcedFont(cnt, "tagged");

                if (tempInfo->availableStatus != asAvailable)
                    ltype->EnableForcedFont(cnt, "inactive");
            }
        } 
        else if (ltype)
        {
            ltype->ResetList();
        }
    } 

    if (lcddev && !lcdItems.isEmpty())
        lcddev->switchToMenu(&lcdItems, lcdTitle);

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

    if (titleList.count() <= 1)
    {
        LayerSet *norec = theme->GetSet("norecordings_list");
        if (type != Delete && norec)
            norec->Draw(&tmp, 8, 0);
        else if (norec)
            norec->Draw(&tmp, 8, 1);
    }
  
    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);

    if (titleList.count() <= 1)
        update(infoRect);
}

void PlaybackBox::cursorLeft()
{
    if (!inTitle)
    {
        if (haveGroupInfoSet)
            killPlayerSafe();
        
        inTitle = true;
        skipUpdate = false;
        
        update(fullRect);
        leftRight = true;
    }
    else if (arrowAccel)
        exitWin();
        
}

void PlaybackBox::cursorRight()
{
    if (inTitle)
    {
        leftRight = true;
        inTitle = false;
        skipUpdate = false;
        update(fullRect);
    }
    else if (curitem && curitem->availableStatus != asAvailable)
        showAvailablePopup(curitem);
    else if (arrowAccel)
        showActionsSelected();    
}

void PlaybackBox::cursorDown(bool page, bool newview)
{
    if (inTitle == true || newview)
    {
        titleIndex += (page ? 5 : 1);
        titleIndex = titleIndex % (int)titleList.count();

        progIndex = 0;

        if (newview)
            inTitle = false;

        skipUpdate = false;
        update(fullRect);
    }
    else 
    {
        int progCount = progLists[titleList[titleIndex]].count();
        if (progIndex < progCount - 1) 
        {
            progIndex += (page ? listsize : 1);
            if (progIndex > progCount - 1)
                progIndex = progCount - 1;

            skipUpdate = false;
            update(fullRect);
        }
    }
}

void PlaybackBox::cursorUp(bool page, bool newview)
{
    if (inTitle == true || newview)
    {
        titleIndex -= (page ? 5 : 1);
        titleIndex += 5 * titleList.count();
        titleIndex = titleIndex % titleList.count();

        progIndex = 0;

        if (newview)
            inTitle = false;

        skipUpdate = false;
        update(fullRect);
    }
    else 
    {
        if (progIndex > 0) 
        {
            progIndex -= (page ? listsize : 1);
            if (progIndex < 0)
                progIndex = 0;

            skipUpdate = false;
            update(fullRect);
        }
    }
}

bool PlaybackBox::FillList()
{
    ProgramInfo *p;

    // Save some information so we can find our place again.
    QString oldtitle = titleList[titleIndex];
    QString oldchanid;
    QString oldprogramid;
    QDate oldoriginalAirDate;
    QDateTime oldstartts;
    p = progLists[oldtitle].at(progIndex);
    if (p)
    {
        oldchanid = p->chanid;
        oldstartts = p->recstartts;
        oldprogramid = p->programid;
        oldoriginalAirDate = p->originalAirDate;
    }

    QMap<QString, AvailableStatusType> asCache;
    QString asKey;
    for (unsigned int i = 0; i < progLists[""].count(); i++)
    {
        p = progLists[""].at(i);
        asKey = p->chanid + ":" + p->recstartts.toString(Qt::ISODate);
        asCache[asKey] = p->availableStatus;
    }

    titleList.clear();
    progLists.clear();
    // Clear autoDelete for the "all" list since it will share the
    // objects with the title lists.
    progLists[""].setAutoDelete(false);

    fillRecGroupPasswordCache();

    QMap<QString, QString> sortedList;
    QRegExp prefixes = tr("^(The |A |An )");
    QString sTitle = "";

    vector<ProgramInfo *> *infoList;
    infoList = RemoteGetRecordedList(listOrder == 0 || type == Delete);
    if (infoList)
    {
        sortedList[""] = "";
        vector<ProgramInfo *>::iterator i = infoList->begin();
        for ( ; i != infoList->end(); i++)
        {
            p = *i;
            if ((((p->recgroup == recGroup) ||
                  (recGroup == "All Programs")) &&
                 (recGroupPassword == curGroupPassword)) ||
                ((recGroupType[recGroup] == "category") &&
                 (p->category == recGroup ) &&
                 ( !recGroupPwCache.contains(p->recgroup))))
            {
                if ((titleView) || (useCategories) || (useRecGroups))
                    progLists[""].prepend(p);

                asKey = p->chanid + ":" + p->recstartts.toString(Qt::ISODate);
                if (asCache.contains(asKey))
                    p->availableStatus = asCache[asKey];
                else
                    p->availableStatus = asAvailable;

                if (titleView) // Normal title view 
                {
                    progLists[p->title].prepend(p);
                    sTitle = p->title;
                    sTitle.remove(prefixes);
                    sortedList[sTitle] = p->title;
                } 

                if (useRecGroups) // Show recording groups                 
                { 
                    progLists[p->recgroup].prepend(p);
                    sortedList[p->recgroup] = p->recgroup;

                    // If another view is also used, unset autodelete as another group will do it.
                    if ((useCategories) || (titleView))
                        progLists[p->recgroup].setAutoDelete(false);
                }

                if (useCategories) // Show categories
                {
                    progLists[p->category].prepend(p);
                    sortedList[p->category] = p->category;
                    // If another view is also used, unset autodelete as another group will do it
                    if ((useRecGroups) || (titleView)) 
                        progLists[p->category].setAutoDelete(false);
                }
            }
            else
                delete p;
        }
        delete infoList;
    }

    if (sortedList.count() == 0)
    {
        progLists[""];
        titleList << "";
        progIndex = 0;
        titleIndex = 0;
        playList.clear();

        return 0;
    }

    titleList = sortedList.values();

    QString episodeSort = gContext->GetSetting("PlayBoxEpisodeSort", "Date");

    if (episodeSort == "OrigAirDate")
    {
        QMap<QString, ProgramList>::Iterator Iprog;
        for (Iprog = progLists.begin(); Iprog != progLists.end(); ++Iprog)
        {
            if (!Iprog.key().isEmpty())
                Iprog.data().Sort(comp_originalAirDate);
        }
    }
    else if (episodeSort == "Id")
    {
        QMap<QString, ProgramList>::Iterator Iprog;
        for (Iprog = progLists.begin(); Iprog != progLists.end(); ++Iprog)
        {
            if (!Iprog.key().isEmpty())
                Iprog.data().Sort(comp_programid);
        }
    }
    
    // Try to find our old place in the title list.  Scan the new
    // titles backwards until we find where we were or go past.  This
    // is somewhat inefficient, but it works.

    QString oldsTitle = oldtitle;
    oldsTitle.remove(prefixes);
    titleIndex = titleList.count() - 1;
    for (int i = titleIndex; i >= 0; i--)
    {
        sTitle = titleList[i];
        sTitle.remove(prefixes);
        
        if (oldsTitle > sTitle)
            break;

        titleIndex = i;

        if (oldsTitle == sTitle)
            break;
    }

    // Now do pretty much the same thing for the individual shows on
    // the specific program list if needed.
    if (oldtitle != titleList[titleIndex] || oldchanid.isNull())
        progIndex = 0;
    else
    {
        ProgramList *l = &progLists[oldtitle];
        progIndex = l->count() - 1;

        for (int i = progIndex; i >= 0; i--)
        {
            p = l->at(i);

            if (episodeSort == "OrigAirDate" && titleIndex > 0)
            {
                if (oldoriginalAirDate > p->originalAirDate)
                    break;
            }
            else if (episodeSort == "Id" && titleIndex > 0)
            {
                if (oldprogramid > p->programid)
                    break;
            }
            else if (listOrder == 0 || type == Delete)
            {
                if (oldstartts > p->recstartts)
                    break;
            }
            else
            {
                if (oldstartts < p->recstartts)
                    break;
            }

            progIndex = i;

            if (oldchanid == p->chanid &&
                oldstartts == p->recstartts)
                break;
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
            VERBOSE(VB_IMPORTANT,
                    "PlaybackBox::startPlayer(): Error, last preview window "
                    "didn't clean up. Not starting a new preview.");
            return;
        }

        rbuffer = new RingBuffer(rec->pathname, false, false);

        nvp = new NuppelVideoPlayer();
        nvp->SetRingBuffer(rbuffer);
        nvp->SetAsPIP();
        QString filters = "";
        nvp->SetVideoFilters(filters);

        everStartedVideo = true;
        pthread_create(&decoder, NULL, SpawnDecoder, nvp);

        nvpTimeout.start();

        state = kStarting;

        int previewRate = 30;
        if (gContext->GetNumSetting("PlaybackPreviewLowCPU", 0))
        {
            previewRate = 12;
        }
        
        timer->start(1000 / previewRate); 
    }
}

void PlaybackBox::playSelectedPlaylist(bool random)
{
    state = kStopping;

    if (!curitem)
        return;

    ProgramInfo *tmpItem;
    QStringList::Iterator it = playList.begin();
    QStringList randomList = playList;
    bool playNext = true;
    int i = 0;
   
    onPlaylist = true;
    while (randomList.count() && playNext)
    {
        if (random)
            i = (int)(1.0 * randomList.count() * rand() / (RAND_MAX + 1.0));

        it = randomList.at(i);
        tmpItem = findMatchingProg(*it);
        if (tmpItem)
        {
            ProgramInfo *rec = new ProgramInfo(*tmpItem);
            if (rec->availableStatus == asAvailable)
                playNext = play(rec);
            delete rec;
        }
        randomList.remove(it);
    }
    onPlaylist = false;
}

void PlaybackBox::playSelected()
{
    state = kStopping;

    if (!curitem)
        return;

    if (curitem && curitem->availableStatus != asAvailable)
        showAvailablePopup(curitem);
    else
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

    if (!playList.count())
    {
        if (curitem->availableStatus != asPendingDelete)
            remove(curitem);
        else
            showAvailablePopup(curitem);
    }
    else
    {
        ProgramInfo *tmpItem;
        QStringList::Iterator it;

        for (it = playList.begin(); it != playList.end(); ++it )
        {
            tmpItem = findMatchingProg(*it);
            if (tmpItem && (tmpItem->availableStatus != asPendingDelete))
                remove(tmpItem);
        }
    }
}

void PlaybackBox::expireSelected()
{
    state = kStopping;

    if (!curitem)
        return;

    expire(curitem);
}

void PlaybackBox::upcoming()
{
    state = kStopping;

    if (!curitem)
        return;

    if (curitem->availableStatus != asAvailable)
    {
        showAvailablePopup(curitem);
        return;
    }

    ProgLister *pl = new ProgLister(plTitle, curitem->title,
                                   gContext->GetMainWindow(), "proglist");
    pl->exec();
    delete pl;
}

void PlaybackBox::details()
{
    state = kStopping;

    if (!curitem)
        return;

    if (curitem->availableStatus != asAvailable)
        showAvailablePopup(curitem);
    else
        curitem->showDetails();
}

void PlaybackBox::selected()
{
    state = kStopping;

    if (inTitle && haveGroupInfoSet)
    {
        cursorRight();
        return;
    }    

    if (!curitem)
        return;

    switch (type) 
    {
        case Play: playSelected(); break;
        case Delete: deleteSelected(); break;
    }
}

void PlaybackBox::showMenu()
{
    state = kStopping;
    killPlayer();

    timer->stop();
    playingVideo = false;

    backup.begin(this);
    grayOut(&backup);
    backup.end();

    popup = new MythPopupBox(gContext->GetMainWindow(), graphicPopup,
                             popupForeground, popupBackground,
                             popupHighlight, "menu popup");

    QLabel *label = popup->addLabel(tr("Recording List Menu"),
                                  MythPopupBox::Large, false);
    label->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    QButton *topButton = popup->addButton(tr("Change Group Filter"), this,
                     SLOT(showRecGroupChooser()));

    popup->addButton(tr("Change Group View"), this, 
                     SLOT(showViewChanger()));

    if (recGroupType[recGroup] == "recgroup")
        popup->addButton(tr("Change Group Password"), this,
                         SLOT(showRecGroupPasswordChanger()));

    if (playList.count())
    {
        popup->addButton(tr("Playlist options"), this,
                         SLOT(showPlaylistPopup()));
    }
    else
    {
        if (inTitle)
        {
            popup->addButton(tr("Add this Group to Playlist"), this,
                             SLOT(togglePlayListTitle()));
        }
        else if (curitem && curitem->availableStatus == asAvailable)
        {
            popup->addButton(tr("Add this recording to Playlist"), this,
                             SLOT(togglePlayListItem()));
        }
    }

    popup->addButton(tr("Cancel"), this, SLOT(doCancel()));

    popup->ShowPopup(this, SLOT(doCancel()));

    topButton->setFocus();

    expectingPopup = true;
}

void PlaybackBox::showActionsSelected()
{
    if (!curitem)
        return;

    if (inTitle && haveGroupInfoSet)
        return;

    if (curitem->availableStatus != asAvailable)
        showAvailablePopup(curitem);
    else
        showActions(curitem);
}

bool PlaybackBox::play(ProgramInfo *rec)
{
    bool playCompleted = false;

    if (!rec)
        return false;

    if (fileExists(rec) == false)
    {
        QString msg = 
            QString("PlaybackBox::play(): Error, %1 file not found")
            .arg(rec->pathname);
        VERBOSE(VB_IMPORTANT, msg);

        ProgramInfo *tmpItem = findMatchingProg(rec);
        if (tmpItem)
        {
            tmpItem->availableStatus = asFileNotFound;
            showAvailablePopup(tmpItem);
        }

        skipUpdate = false;
        update(fullRect);
        return false;
    }

    ProgramInfo *tvrec = new ProgramInfo(*rec);

    TV *tv = new TV();
    if (!tv->Init())
    {
        VERBOSE(VB_IMPORTANT, "PlaybackBox::play(): "
                "Error, initializing TV class.");
        delete tv;
        delete tvrec;
        return false;
    }

    setEnabled(false);
    state = kKilling; // stop preview playback and don't restart it
    playingSomething = true;

    tv->setLastProgram(lastProgram);

    if (tv->Playback(tvrec))
    {
        while (tv->GetState() != kState_None)
        {
            qApp->unlock();
            qApp->processEvents();
            usleep(100000);
            qApp->lock();
        }
        while (tv->getJumpToProgram())
        {
            ProgramInfo *tmpProgram = new ProgramInfo(*lastProgram);

            if (lastProgram)
                delete lastProgram;
            lastProgram = new ProgramInfo(*tvrec);

            if (tvrec)
                delete tvrec;
            tvrec = tmpProgram;

            if (tv->Playback(tvrec))
            {
                while (tv->GetState() != kState_None)
                {
                    qApp->unlock();
                    qApp->processEvents();
                    usleep(100000);
                    qApp->lock();
                }
            }
        }
    }

    if (lastProgram)
        delete lastProgram;

    lastProgram = new ProgramInfo(*tvrec);

    playingSomething = false;
    state = kStarting; // restart playback preview
    setEnabled(true);

    update(fullRect);

    bool doremove = false;
    bool doprompt = false;

    if (tv->getEndOfRecording())
        playCompleted = true;

    if (tv->getRequestDelete())
    {
        doremove = true;
    }
    else if (tv->getEndOfRecording() &&
             !onPlaylist &&
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

    return playCompleted;
}

void PlaybackBox::stop(ProgramInfo *rec)
{
    RemoteStopRecording(rec);
}

bool PlaybackBox::doRemove(ProgramInfo *rec, bool forgetHistory,
                           bool forceMetadataDelete)
{
    return RemoteDeleteRecording(rec, forgetHistory, forceMetadataDelete);
}

void PlaybackBox::remove(ProgramInfo *toDel)
{
    state = kStopping;

    if (delitem)
        delete delitem;

    delitem = new ProgramInfo(*toDel);
    showDeletePopup(delitem, DeleteRecording);
}

void PlaybackBox::expire(ProgramInfo *toExp)
{
    state = kStopping;

    if (delitem)
        delete delitem;
    delitem = new ProgramInfo(*toExp);

    showDeletePopup(delitem, AutoExpireRecording);
}

void PlaybackBox::showActions(ProgramInfo *toExp)
{
    killPlayer();

    if (delitem)
        delete delitem;

    delitem = new ProgramInfo(*toExp);

    if (delitem->availableStatus != asAvailable)
        showAvailablePopup(delitem);
    else
        showActionPopup(delitem);
}

void PlaybackBox::showDeletePopup(ProgramInfo *program, deletePopupType types)
{
    updateFreeSpace = true;

    backup.begin(this);
    grayOut(&backup);
    backup.end();

    popup = new MythPopupBox(gContext->GetMainWindow(), graphicPopup,
                             popupForeground, popupBackground,
                             popupHighlight, "delete popup");
    QString message1 = "";
    QString message2 = "";
    switch (types)
    {
        case EndOfRecording:
             message1 = tr("You have finished watching:"); break;
        case DeleteRecording:
             message1 = tr("Are you sure you want to delete:"); break;
        case ForceDeleteRecording:
             message1 = tr("ERROR: Recorded file does not exist.");
             message2 = tr("Are you sure you want to delete:");
             break;
        case AutoExpireRecording:
             message1 = tr("Allow this program to AutoExpire?"); break;
        case StopRecording:
             message1 = tr("Are you sure you want to stop:"); break;
    }
    
    initPopup(popup, program, message1, message2);

    QString tmpmessage;
    const char *tmpslot = NULL;

    if ((types == EndOfRecording || types == DeleteRecording) &&
        (program->IsSameProgram(*program)))
    {
        if (types == EndOfRecording)
            tmpmessage = tr("Delete it, but allow it to re-record"); 
        else
            tmpmessage = tr("Yes, and allow re-record"); 
        tmpslot = SLOT(doDeleteForgetHistory());
        popup->addButton(tmpmessage, this, tmpslot);
    }

    switch (types)
    {
        case EndOfRecording:
             tmpmessage = tr("Delete it"); 
             tmpslot = SLOT(doDelete());
             break;
        case DeleteRecording:
             tmpmessage = tr("Yes, delete it"); 
             tmpslot = SLOT(doDelete());
             break;
        case ForceDeleteRecording:
             tmpmessage = tr("Yes, delete it"); 
             tmpslot = SLOT(doForceDelete());
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
             tmpmessage = tr("Save it so I can watch it again"); 
             tmpslot = SLOT(noDelete());
             break;
        case DeleteRecording:
        case ForceDeleteRecording:
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

    if (types == EndOfRecording || types == DeleteRecording ||
        types == ForceDeleteRecording)
        noButton->setFocus();
    else
    {
        if (program->GetAutoExpireFromRecorded())
            yesButton->setFocus();
        else
            noButton->setFocus();
    }
 
    popup->ShowPopup(this, SLOT(doCancel())); 

    expectingPopup = true;
}

void PlaybackBox::showAvailablePopup(ProgramInfo *rec)
{
    switch (rec->availableStatus)
    {
        case asAvailable:
                 MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                               QObject::tr("Recording Unavailable"),
                               QObject::tr("This recording is currently "
                                           "Available"));
                 break;
        case asPendingDelete:
                 MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                               QObject::tr("Recording Unavailable"),
                               QObject::tr("This recording is currently being "
                                           "deleted and is unavailable"));
                 break;
        case asFileNotFound:
                 MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                               QObject::tr("Recording Unavailable"),
                               QObject::tr("The file for this recording can "
                                           "not be found"));
                 break;
    }
}

void PlaybackBox::showPlaylistPopup()
{
    if (expectingPopup)
       cancelPopup();

    backup.begin(this);
    grayOut(&backup);
    backup.end();

    popup = new MythPopupBox(gContext->GetMainWindow(), graphicPopup,
                             popupForeground, popupBackground,
                             popupHighlight, "playlist popup");

    QLabel *tlabel = NULL;
    if (playList.count() > 1)
    {
        tlabel = popup->addLabel(tr("There are %1 items in the playlist.")
                                       .arg(playList.count()));
    } else {
        tlabel = popup->addLabel(tr("There is %1 item in the playlist.")
                                       .arg(playList.count()));
    }
    tlabel->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    QButton *playButton = popup->addButton(tr("Play"), this, SLOT(doPlayList()));
    
    popup->addButton(tr("Shuffle Play"), this, SLOT(doPlayListRandom()));
    popup->addButton(tr("Clear Playlist"), this, SLOT(doClearPlaylist()));
 
    if (inTitle)
    {
        if (titleView)
            popup->addButton(tr("Toggle playlist for this Category/Title"),
                             this, SLOT(togglePlayListTitle()));
        else 
            popup->addButton(tr("Toggle playlist for this Recording Group"),
                             this, SLOT(togglePlayListTitle()));
    }
    else
    {
        popup->addButton(tr("Toggle playlist for this recording"), this,
                         SLOT(togglePlayListItem()));
    }

    QLabel *label = popup->addLabel(
                        tr("These actions affect all items in the playlist"));
    label->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    popup->addButton(tr("Change Recording Group"), this,
                     SLOT(doPlaylistChangeRecGroup()));
    popup->addButton(tr("Job Options"), this,
                     SLOT(showPlaylistJobPopup()));
    popup->addButton(tr("Delete"), this, SLOT(doPlaylistDelete()));

    playButton->setFocus();

    popup->ShowPopup(this, SLOT(doCancel()));
  
    expectingPopup = true;
}

void PlaybackBox::showPlaylistJobPopup()
{
    if (expectingPopup)
       cancelPopup();

    backup.begin(this);
    grayOut(&backup);
    backup.end();
    
    popup = new MythPopupBox(gContext->GetMainWindow(), graphicPopup,
                             popupForeground, popupBackground,
                             popupHighlight, "playlist popup");

    QLabel *tlabel = NULL;
    if (playList.count() > 1)
    {
        tlabel = popup->addLabel(tr("There are %1 items in the playlist.")
                                       .arg(playList.count()));
    } else {
        tlabel = popup->addLabel(tr("There is %1 item in the playlist.")
                                       .arg(playList.count()));
    }
    tlabel->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    QButton *jobButton;
    QString jobTitle = "";
    QString command = "";
    QStringList::Iterator it;
    ProgramInfo *tmpItem;
    bool isTranscoding = true;
    bool isFlagging = true;
    bool isRunningUserJob1 = true;
    bool isRunningUserJob2 = true;
    bool isRunningUserJob3 = true;
    bool isRunningUserJob4 = true;

    for(it = playList.begin(); it != playList.end(); ++it)
    {
        tmpItem = findMatchingProg(*it);
        if (tmpItem) {
            if (!JobQueue::IsJobQueuedOrRunning(JOB_TRANSCODE,
                                       tmpItem->chanid, tmpItem->recstartts))
                isTranscoding = false;
            if (!JobQueue::IsJobQueuedOrRunning(JOB_COMMFLAG,
                                       tmpItem->chanid, tmpItem->recstartts))
                isFlagging = false;
            if (!JobQueue::IsJobQueuedOrRunning(JOB_USERJOB1,
                                       tmpItem->chanid, tmpItem->recstartts))
                isRunningUserJob1 = false;
            if (!JobQueue::IsJobQueuedOrRunning(JOB_USERJOB2,
                                       tmpItem->chanid, tmpItem->recstartts))
                isRunningUserJob2 = false;
            if (!JobQueue::IsJobQueuedOrRunning(JOB_USERJOB3,
                                       tmpItem->chanid, tmpItem->recstartts))
                isRunningUserJob3 = false;
            if (!JobQueue::IsJobQueuedOrRunning(JOB_USERJOB4,
                                       tmpItem->chanid, tmpItem->recstartts))
                isRunningUserJob4 = false;
            if (!isTranscoding && !isFlagging && !isRunningUserJob1 &&
                !isRunningUserJob2 && !isRunningUserJob3 && !isRunningUserJob4)
                break;
        }
    }
    if (!isTranscoding)
        jobButton = popup->addButton(tr("Begin Transcoding"), this,
                         SLOT(doPlaylistBeginTranscoding()));
    else
        jobButton = popup->addButton(tr("Stop Transcoding"), this,
                         SLOT(stopPlaylistTranscoding()));
    if (!isFlagging)
        popup->addButton(tr("Begin Commercial Flagging"), this,
                         SLOT(doPlaylistBeginFlagging()));
    else
        popup->addButton(tr("Stop Commercial Flagging"), this,
                         SLOT(stopPlaylistFlagging()));

    command = gContext->GetSetting("UserJob1", "");
    if (command != "") {
        jobTitle = gContext->GetSetting("UserJobDesc1");

        if (!isRunningUserJob1)
            popup->addButton(tr("Begin") + " " + jobTitle, this,
                             SLOT(doPlaylistBeginUserJob1()));
        else
            popup->addButton(tr("Stop") + " " + jobTitle, this,
                             SLOT(stopPlaylistUserJob1()));
    }
    
    command = gContext->GetSetting("UserJob2", "");
    if (command != "") {
        jobTitle = gContext->GetSetting("UserJobDesc2");

        if (!isRunningUserJob2)
            popup->addButton(tr("Begin") + " " + jobTitle, this,
                             SLOT(doPlaylistBeginUserJob2()));
        else
            popup->addButton(tr("Stop") + " " + jobTitle, this,
                             SLOT(stopPlaylistUserJob2()));
    }
    
    command = gContext->GetSetting("UserJob3", "");
    if (command != "") {
        jobTitle = gContext->GetSetting("UserJobDesc3");

        if (!isRunningUserJob3)
            popup->addButton(tr("Begin") + " " + jobTitle, this,
                             SLOT(doPlaylistBeginUserJob3()));
        else
            popup->addButton(tr("Stop") + " " + jobTitle, this,
                             SLOT(stopPlaylistUserJob3()));
    }
    
    command = gContext->GetSetting("UserJob4", "");
    if (command != "") {
        jobTitle = gContext->GetSetting("UserJobDesc4");

        if (!isRunningUserJob4)
            popup->addButton(tr("Begin") + " " + jobTitle, this,
                             SLOT(doPlaylistBeginUserJob4()));
        else
            popup->addButton(tr("Stop") + " " + jobTitle, this,
                             SLOT(stopPlaylistUserJob4()));
    }
    
    popup->ShowPopup(this, SLOT(doCancel()));
    jobButton->setFocus();

    expectingPopup = true;
}

void PlaybackBox::showPlayFromPopup()
{
    if (expectingPopup)
        cancelPopup();

    backup.begin(this);
    grayOut(&backup);
    backup.end();

    popup = new MythPopupBox(gContext->GetMainWindow(), graphicPopup,
                             popupForeground, popupBackground,
                             popupHighlight, "playfrom popup");

    initPopup(popup, delitem, "", "");

    QButton *playButton = popup->addButton(tr("Play from bookmark"), this,
            SLOT(doPlay()));
    popup->addButton(tr("Play from beginning"), this, SLOT(doPlayFromBeg()));
     
    popup->ShowPopup(this, SLOT(doCancel()));
    playButton->setFocus();
    
    expectingPopup = true;
}

void PlaybackBox::showStoragePopup()
{
    if (expectingPopup)
        cancelPopup();

    backup.begin(this);
    grayOut(&backup);
    backup.end();

    popup = new MythPopupBox(gContext->GetMainWindow(), graphicPopup,
                             popupForeground, popupBackground,
                             popupHighlight, "storage popup");

    initPopup(popup, delitem, "", tr("A preserved episode is ignored in calculations for deleting episodes above the limit.  Auto-expiration is used to remove eligable programs when disk space is low."));

    QButton *storageButton;

    if (delitem && delitem->GetAutoExpireFromRecorded())
        storageButton = popup->addButton(tr("Don't Auto Expire"), this, SLOT(noAutoExpire()));
    else
        storageButton = popup->addButton(tr("Auto Expire"), this, SLOT(doAutoExpire()));

    if (delitem && delitem->UsesMaxEpisodes())
    {
        if (delitem && delitem->GetPreserveEpisodeFromRecorded())
            popup->addButton(tr("Do not preserve this episode"), this, SLOT(noPreserveEpisode()));
        else
            popup->addButton(tr("Preserve this episode"), this, SLOT(doPreserveEpisode()));
    }

    popup->ShowPopup(this, SLOT(doCancel()));
    storageButton->setFocus();

    expectingPopup = true;
}

void PlaybackBox::showRecordingPopup()
{
    if (expectingPopup)
        cancelPopup();

    backup.begin(this);
    grayOut(&backup);
    backup.end();

    popup = new MythPopupBox(gContext->GetMainWindow(), graphicPopup,
                             popupForeground, popupBackground,
                             popupHighlight, "recording popup");

    initPopup(popup, delitem, "", "");

    QButton *changeButton = popup->addButton(tr("Change Recording Group"), this,
                     SLOT(showRecGroupChanger()));

    popup->addButton(tr("Edit Recording Schedule"), this,
                     SLOT(doEditScheduled()));
    
    popup->ShowPopup(this, SLOT(doCancel()));
    changeButton->setFocus();
    
    expectingPopup = true;
}

void PlaybackBox::showJobPopup()
{
    if (expectingPopup)
        cancelPopup();

    if (!curitem)
        return;

    backup.begin(this);
    grayOut(&backup);
    backup.end();

    popup = new MythPopupBox(gContext->GetMainWindow(), graphicPopup,
                             popupForeground, popupBackground,
                             popupHighlight, "job popup");

    initPopup(popup, delitem, "", "");

    QButton *jobButton;
    QString jobTitle = "";
    QString command = "";

    if (JobQueue::IsJobQueuedOrRunning(JOB_TRANSCODE, curitem->chanid,
                                                  curitem->recstartts))
        jobButton = popup->addButton(tr("Stop Transcoding"), this,
                         SLOT(doBeginTranscoding()));
    else
        jobButton = popup->addButton(tr("Begin Transcoding"), this,
                         SLOT(doBeginTranscoding()));

    if (JobQueue::IsJobQueuedOrRunning(JOB_COMMFLAG, curitem->chanid,
                                                  curitem->recstartts))
        popup->addButton(tr("Stop Commercial Flagging"), this,
                         SLOT(doBeginFlagging()));
    else
        popup->addButton(tr("Begin Commercial Flagging"), this,
                         SLOT(doBeginFlagging()));

    command = gContext->GetSetting("UserJob1", "");
    if (command != "") {
        jobTitle = gContext->GetSetting("UserJobDesc1", tr("User Job") + " #1");

        if (JobQueue::IsJobQueuedOrRunning(JOB_USERJOB1, curitem->chanid,
                                   curitem->recstartts))
            popup->addButton(tr("Stop") + " " + jobTitle, this,
                             SLOT(doBeginUserJob1()));
        else
            popup->addButton(tr("Begin") + " " + jobTitle, this,
                             SLOT(doBeginUserJob1()));
    }

    command = gContext->GetSetting("UserJob2", "");
    if (command != "") {
        jobTitle = gContext->GetSetting("UserJobDesc2", tr("User Job") + " #2");

        if (JobQueue::IsJobQueuedOrRunning(JOB_USERJOB2, curitem->chanid,
                                   curitem->recstartts))
            popup->addButton(tr("Stop") + " " + jobTitle, this,
                             SLOT(doBeginUserJob2()));
        else
            popup->addButton(tr("Begin") + " " + jobTitle, this,
                             SLOT(doBeginUserJob2()));
    }

    command = gContext->GetSetting("UserJob3", "");
    if (command != "") {
        jobTitle = gContext->GetSetting("UserJobDesc3", tr("User Job") + " #3");

        if (JobQueue::IsJobQueuedOrRunning(JOB_USERJOB3, curitem->chanid,
                                   curitem->recstartts))
            popup->addButton(tr("Stop") + " " + jobTitle, this,
                             SLOT(doBeginUserJob3()));
        else
            popup->addButton(tr("Begin") + " " + jobTitle, this,
                             SLOT(doBeginUserJob3()));
    }

    command = gContext->GetSetting("UserJob4", "");
    if (command != "") {
        jobTitle = gContext->GetSetting("UserJobDesc4", tr("User Job") + " #4");

        if (JobQueue::IsJobQueuedOrRunning(JOB_USERJOB4, curitem->chanid,
                                   curitem->recstartts))
            popup->addButton(tr("Stop") + " " + jobTitle, this,
                             SLOT(doBeginUserJob4()));
        else
            popup->addButton(tr("Begin") + " "  + jobTitle, this,
                             SLOT(doBeginUserJob4()));
    }

    popup->ShowPopup(this, SLOT(doCancel()));
    jobButton->setFocus();
    
    expectingPopup = true;
}

void PlaybackBox::showActionPopup(ProgramInfo *program)
{
    if (!curitem || !program)
        return;

    backup.begin(this);
    grayOut(&backup);
    backup.end();

    popup = new MythPopupBox(gContext->GetMainWindow(), graphicPopup,
                             popupForeground, popupBackground,
                             popupHighlight, "action popup");

    initPopup(popup, program, "", "");

    QButton *playButton;

    if (curitem->programflags & FL_BOOKMARK)
    {
        playButton = popup->addButton(tr("Play from..."), this,
                                      SLOT(showPlayFromPopup()));
    }
    else
    {
        playButton = popup->addButton(tr("Play"), this, SLOT(doPlay()));
    }

    QString key;
    key = curitem->chanid + "_" +
         curitem->recstartts.toString(Qt::ISODate);

    if (playList.grep(key).count())
        popup->addButton(tr("Remove from Playlist"), this,
                         SLOT(togglePlayListItem()));
    else
        popup->addButton(tr("Add to Playlist"), this,
                         SLOT(togglePlayListItem()));

    if (program->recstatus == rsRecording)
        popup->addButton(tr("Stop Recording"), this, SLOT(askStop()));

    // Remove this check and the auto expire buttons if a third button is added
    // to the StoragePopup screen.  Otherwise for non-max-episode schedules,
    // the popup will only show one button
    if (delitem && delitem->UsesMaxEpisodes())
    {
        popup->addButton(tr("Storage Options"), this, SLOT(showStoragePopup()));
    } else {
        if (delitem && delitem->GetAutoExpireFromRecorded())
            popup->addButton(tr("Don't Auto Expire"), this,
                             SLOT(noAutoExpire()));
        else
            popup->addButton(tr("Auto Expire"), this, SLOT(doAutoExpire()));
    }

    popup->addButton(tr("Recording Options"), this, SLOT(showRecordingPopup()));
    popup->addButton(tr("Job Options"), this, SLOT(showJobPopup()));

    popup->addButton(tr("Delete"), this, SLOT(askDelete()));

    popup->ShowPopup(this, SLOT(doCancel()));

    playButton->setFocus();

    expectingPopup = true;
}

void PlaybackBox::initPopup(MythPopupBox *popup, ProgramInfo *program,
                            QString message, QString message2)
{
    killPlayerSafe();

    QDateTime recstartts = program->recstartts;
    QDateTime recendts = program->recendts;

    QString timedate = recstartts.date().toString(dateformat) + QString(", ") +
                       recstartts.time().toString(timeformat) + QString(" - ") +
                       recendts.time().toString(timeformat);

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

void PlaybackBox::cancelPopup(void)
{
    popup->hide();
    expectingPopup = false;

    backup.begin(this);
    backup.drawPixmap(0, 0, myBackground);
    backup.end();

    delete popup;
    popup = NULL;

    skipUpdate = false;
    skipCnt = 2;
    update(fullRect);

    setActiveWindow();
}

void PlaybackBox::doClearPlaylist(void)
{
    if (expectingPopup)
        cancelPopup();

    playList.clear();
}

void PlaybackBox::doPlay(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    if (curitem)
        delete curitem;

    curitem = new ProgramInfo(*delitem);

    playSelected();
}

void PlaybackBox::doPlayFromBeg(void)
{
    delitem->setIgnoreBookmark(true);
    doPlay();
}

void PlaybackBox::doPlayList(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    playSelectedPlaylist(false);
}


void PlaybackBox::doPlayListRandom(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    playSelectedPlaylist(true);
}

void PlaybackBox::doPreserveEpisode(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    if (delitem->availableStatus != asAvailable)
        showAvailablePopup(delitem);
    else
        delitem->SetPreserveEpisode(true);
}

void PlaybackBox::noPreserveEpisode(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    if (delitem->availableStatus != asAvailable)
        showAvailablePopup(delitem);
    else
        delitem->SetPreserveEpisode(false);
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

    if (!curitem)
        return;

    if (curitem->availableStatus != asAvailable)
    {
        showAvailablePopup(curitem);
    }
    else
    {
        ScheduledRecording record;
        record.loadByProgram(curitem);
        record.exec();
    
        connected = FillList();
        update(fullRect);
    }
}    

void PlaybackBox::doJobQueueJob(int jobType, int jobFlags)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    if (!curitem)
        return;

    ProgramInfo *tmpItem = findMatchingProg(curitem);

    if (JobQueue::IsJobQueuedOrRunning(jobType, curitem->chanid,
                               curitem->recstartts))
    {
        JobQueue::ChangeJobCmds(jobType, curitem->chanid,
                                curitem->recstartts, JOB_STOP);
        if ((jobType & JOB_COMMFLAG) && (tmpItem))
        {
            tmpItem->programflags &= ~FL_EDITING;
            tmpItem->programflags &= ~FL_COMMFLAG;
        }
    } else {
        QString jobHost = "";
        if (gContext->GetNumSetting("JobsRunOnRecordHost", 0))
            jobHost = curitem->hostname;

        JobQueue::QueueJob(jobType, curitem->chanid, curitem->recstartts,
                           "", "", jobHost, jobFlags);
    }
}

void PlaybackBox::doBeginTranscoding()
{
    doJobQueueJob(JOB_TRANSCODE, JOB_USE_CUTLIST);
}

void PlaybackBox::doBeginFlagging()
{
    doJobQueueJob(JOB_COMMFLAG);
    update(listRect);
}

void PlaybackBox::doBeginUserJob1()
{
    doJobQueueJob(JOB_USERJOB1);
}

void PlaybackBox::doBeginUserJob2()
{
    doJobQueueJob(JOB_USERJOB2);
}

void PlaybackBox::doBeginUserJob3()
{
    doJobQueueJob(JOB_USERJOB3);
}

void PlaybackBox::doBeginUserJob4()
{
    doJobQueueJob(JOB_USERJOB4);
}

void PlaybackBox::doPlaylistJobQueueJob(int jobType, int jobFlags)
{
    ProgramInfo *tmpItem;
    QStringList::Iterator it;
    int jobID;

    if (expectingPopup)
        cancelPopup();

    for (it = playList.begin(); it != playList.end(); ++it)
    {
        jobID = 0;
        tmpItem = findMatchingProg(*it);
        if (tmpItem && 
            (!JobQueue::IsJobQueuedOrRunning(jobType, tmpItem->chanid,
                                    tmpItem->recstartts))) {
            QString jobHost = "";
            if (gContext->GetNumSetting("JobsRunOnRecordingHost", 0))
                jobHost = gContext->GetHostName();

            JobQueue::QueueJob(jobType, tmpItem->chanid,
                               tmpItem->recstartts, "", "", jobHost, jobFlags);
        }
    }
}

void PlaybackBox::stopPlaylistJobQueueJob(int jobType)
{
    ProgramInfo *tmpItem;
    QStringList::Iterator it;
    int jobID;

    if (expectingPopup)
        cancelPopup();

    for (it = playList.begin(); it != playList.end(); ++it)
    {
        jobID = 0;
        tmpItem = findMatchingProg(*it);
        if (tmpItem &&
            (JobQueue::IsJobQueuedOrRunning(jobType, tmpItem->chanid,
                                tmpItem->recstartts)))
        {
cout << "stop job " << jobType << ": job is queued or running\n";
            JobQueue::ChangeJobCmds(jobType, tmpItem->chanid,
                                     tmpItem->recstartts, JOB_STOP);
            if ((jobType & JOB_COMMFLAG) && (tmpItem))
            {
                tmpItem->programflags &= ~FL_EDITING;
                tmpItem->programflags &= ~FL_COMMFLAG;
            }
        } else 
cout << "stop job tmpItem == NULL\n";
    }
}

void PlaybackBox::doPlaylistBeginTranscoding()
{
    doPlaylistJobQueueJob(JOB_TRANSCODE, JOB_USE_CUTLIST);
}

void PlaybackBox::stopPlaylistTranscoding()
{
    stopPlaylistJobQueueJob(JOB_TRANSCODE);
}

void PlaybackBox::doPlaylistBeginFlagging()
{
    doPlaylistJobQueueJob(JOB_COMMFLAG);
}

void PlaybackBox::stopPlaylistFlagging()
{
    stopPlaylistJobQueueJob(JOB_COMMFLAG);
}

void PlaybackBox::doPlaylistBeginUserJob1()
{
    doPlaylistJobQueueJob(JOB_USERJOB1);
}

void PlaybackBox::stopPlaylistUserJob1()
{
    stopPlaylistJobQueueJob(JOB_USERJOB1);
}

void PlaybackBox::doPlaylistBeginUserJob2()
{
    doPlaylistJobQueueJob(JOB_USERJOB1);
}

void PlaybackBox::stopPlaylistUserJob2()
{
    stopPlaylistJobQueueJob(JOB_USERJOB2);
}

void PlaybackBox::doPlaylistBeginUserJob3()
{
    doPlaylistJobQueueJob(JOB_USERJOB1);
}

void PlaybackBox::stopPlaylistUserJob3()
{
    stopPlaylistJobQueueJob(JOB_USERJOB3);
}

void PlaybackBox::doPlaylistBeginUserJob4()
{
    doPlaylistJobQueueJob(JOB_USERJOB1);
}

void PlaybackBox::stopPlaylistUserJob4()
{
    stopPlaylistJobQueueJob(JOB_USERJOB4);
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

void PlaybackBox::doPlaylistDelete(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    ProgramInfo *tmpItem;
    QStringList::Iterator it;

    for (it = playList.begin(); it != playList.end(); ++it )
    {
        tmpItem = findMatchingProg(*it);
        if (tmpItem)
            RemoteDeleteRecording(tmpItem, false, false);
    }

    connected = FillList();
    update(fullRect);
    playList.clear();
}

void PlaybackBox::doDelete(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    if (delitem->availableStatus == asPendingDelete)
    {
        showAvailablePopup(delitem);
        return;
    }

    bool result = doRemove(delitem, false, false);

    state = kChanging;

    timer->start(500);

    if (result)
    {
        ProgramInfo *tmpItem = findMatchingProg(delitem);
        if (tmpItem)
            tmpItem->availableStatus = asPendingDelete;
    }
    else
        showDeletePopup(delitem, ForceDeleteRecording);
}

void PlaybackBox::doForceDelete(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    if (delitem->availableStatus == asPendingDelete)
    {
        showAvailablePopup(delitem);
        return;
    }

    doRemove(delitem, true, true);

    state = kChanging;

    timer->start(500);
}

void PlaybackBox::doDeleteForgetHistory(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    if (delitem->availableStatus == asPendingDelete)
    {
        showAvailablePopup(delitem);
        return;
    }

    bool result = doRemove(delitem, true, false);

    state = kChanging;

    timer->start(500);

    if (result)
    {
        ProgramInfo *tmpItem = findMatchingProg(delitem);
        if (tmpItem)
            tmpItem->availableStatus = asPendingDelete;
    }
    else
        showDeletePopup(delitem, ForceDeleteRecording);
}

ProgramInfo *PlaybackBox::findMatchingProg(ProgramInfo *pginfo)
{
    ProgramInfo *p;
    ProgramList *l = &progLists[""];

    for (p = l->first(); p; p = l->next())
    {
        if (p->recstartts == pginfo->recstartts &&
            p->chanid == pginfo->chanid)
            return p;
    }

    return NULL;
}

ProgramInfo *PlaybackBox::findMatchingProg(QString key)
{
    QStringList keyParts;

    if ((key == "") || (key.find('_') < 0))
        return NULL;

    keyParts = QStringList::split("_", key);

    if (keyParts.count() == 2)
        return findMatchingProg(keyParts[0], keyParts[1]);
    else
        return NULL;
}

ProgramInfo *PlaybackBox::findMatchingProg(QString chanid, QString recstartts)
{
    ProgramInfo *p;
    ProgramList *l = &progLists[""];

    for (p = l->first(); p; p = l->next())
    {
        if (p->recstartts.toString(Qt::ISODate) == recstartts &&
            p->chanid == chanid)
            return p;
    }

    return NULL;
}

void PlaybackBox::noAutoExpire(void)
{
    if (!expectingPopup && delitem)
        return;

    cancelPopup();

    delitem->SetAutoExpire(false);

    ProgramInfo *tmpItem = findMatchingProg(delitem);
    if (tmpItem)
        tmpItem->programflags &= ~FL_AUTOEXP;

    delete delitem;
    delitem = NULL;

    state = kChanging;

    update(listRect);
}

void PlaybackBox::doAutoExpire(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    delitem->SetAutoExpire(true);

    ProgramInfo *tmpItem = findMatchingProg(delitem);
    if (tmpItem)
        tmpItem->programflags |= FL_AUTOEXP;

    delete delitem;
    delitem = NULL;

    state = kChanging;

    update(listRect);
}

void PlaybackBox::doCancel(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    state = kChanging;
}

void PlaybackBox::toggleTitleView(void)
{
    if (expectingPopup)
        cancelPopup();

    if (titleView) titleView = false;
    else titleView = true;

    playList.clear();
    connected = FillList();      
    skipUpdate = false;
    update(fullRect);
}

void PlaybackBox::promptEndOfRecording(ProgramInfo *rec)
{
    if (!rec)
        return;

    state = kStopping;

    if (!rec)
        return;

    if (delitem)
        delete delitem;
        
    delitem = new ProgramInfo(*rec);
    showDeletePopup(delitem, EndOfRecording);
}

void PlaybackBox::UpdateProgressBar(void)
{
    update(usageRect);
}

void PlaybackBox::togglePlayListTitle(void)
{
    if (expectingPopup)
        cancelPopup();

    if (!curitem)
        return;

    QString currentTitle = titleList[titleIndex];
    ProgramInfo *p;

    for( unsigned int i = 0; i < progLists[currentTitle].count(); i++)
    {
        p = progLists[currentTitle].at(i);
        if (p && (p->availableStatus == asAvailable))
            togglePlayListItem(p);
    }

    if (inTitle)
        cursorRight();
}

void PlaybackBox::togglePlayListItem(void)
{
    if (expectingPopup)
        cancelPopup();

    if (!curitem)
        return;

    if (curitem->availableStatus != asAvailable)
    {
        showAvailablePopup(curitem);
        return;
    }

    togglePlayListItem(curitem);

    if (!inTitle)
        cursorDown();

    skipUpdate = false;
    update(fullRect);
}

void PlaybackBox::togglePlayListItem(ProgramInfo *pginfo)
{
    QString key;

    if (!pginfo)
        return;

    if (pginfo->availableStatus != asAvailable)
    {
        showAvailablePopup(pginfo);
        return;
    }

    key  = pginfo->chanid + "_" + pginfo->recstartts.toString(Qt::ISODate);

    if (playList.grep(key).count())
    {
        QStringList tmpList;
        QStringList::Iterator it;

        tmpList = playList;
        playList.clear();

        for (it = tmpList.begin(); it != tmpList.end(); ++it )
        {
            if (*it != key)
                playList << *it;
        }
    }
    else
    {
        playList << key;
    }
}

void PlaybackBox::timeout(void)
{
    if (titleList.count() <= 1)
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
        else if (action == "NEXTFAV")
        {
            if (inTitle)
                togglePlayListTitle();
            else
                togglePlayListItem();
        }
        else if (action == "TOGGLEFAV")
        {
            playList.clear();
            connected = FillList();      
            skipUpdate = false;
            update(fullRect);
        }
        else if (action == "TOGGLERECORD")
        {
            if (titleView) titleView = false;
            else titleView = true;
            connected = FillList();
            skipUpdate = false;
            update(fullRect);
        }
        else if (titleList.count() > 1)
        {
            if (action == "DELETE")
                deleteSelected();
            else if (action == "PLAYBACK")
                playSelected();
            else if (action == "INFO")
                showActionsSelected();
            else if (action == "DETAILS")
                details();
            else if (action == "UPCOMING")
                upcoming();
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
            connected = FillList();      
            skipUpdate = false;
            update(fullRect);
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

QDateTime PlaybackBox::getPreviewLastModified(ProgramInfo *pginfo)
{
    QString filename = pginfo->pathname;
    filename += ".png";

    if (filename.left(7) == "myth://")
    {
        Qt::DateFormat f = Qt::TextDate;
        QString filetime = RemoteGetPreviewLastModified(pginfo);
        QDateTime retLastModified = QDateTime::fromString(filetime,f);
        return retLastModified;
    }

    QFileInfo retfinfo(filename);

    return retfinfo.lastModified();
}

QPixmap PlaybackBox::getPixmap(ProgramInfo *pginfo)
{
    QPixmap retpixmap;

    if (!generatePreviewPixmap || !pginfo)
        return retpixmap;
        
    QString filename = pginfo->pathname;
    filename += ".png";

    previewLastModified = getPreviewLastModified(pginfo);
    if (previewLastModified <  pginfo->lastmodified &&
        previewLastModified >= pginfo->recendts &&
        !pginfo->IsEditing() && 
        !pginfo->IsCommProcessing())
    {
        RemoteGeneratePreviewPixmap(pginfo);
        previewLastModified = getPreviewLastModified(pginfo);
    }

    // Check and see if we've already tried this one.
    if (pginfo->recstartts == previewStartts &&
        pginfo->chanid == previewChanid &&
        previewLastModified == previewFilets)
    {
        if (previewPixmap)
            retpixmap = *previewPixmap;
        
        return retpixmap;
    }

    if (previewPixmap)
    {
        delete previewPixmap;
        previewPixmap = NULL;
    }
    
    int screenheight = 0, screenwidth = 0;
    float wmult = 0, hmult = 0;

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    previewPixmap = gContext->LoadScalePixmap(filename);
    if (previewPixmap)
    {
        previewStartts = pginfo->recstartts;
        previewChanid = pginfo->chanid;
        previewFilets = previewLastModified;
        retpixmap = *previewPixmap;
        return retpixmap;
    }

    //if this is a remote frontend, then we need to refresh the pixmap
    //if another frontend has already regenerated the stale pixmap on the disk
    bool refreshPixmap = false;    
    if (previewLastModified >= previewFilets)
        refreshPixmap = true;    

    QImage *image = gContext->CacheRemotePixmap(filename, refreshPixmap);

    if (!image)
    {
        RemoteGeneratePreviewPixmap(pginfo);

        previewPixmap = gContext->LoadScalePixmap(filename);
        if (previewPixmap)
        {
            retpixmap = *previewPixmap;
            previewStartts = pginfo->recstartts;
            previewChanid = pginfo->chanid;
            previewFilets = previewLastModified;
            return retpixmap;
        }

        image = gContext->CacheRemotePixmap(filename, refreshPixmap);
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
    previewStartts = pginfo->recstartts;
    previewChanid = pginfo->chanid;
    previewFilets = previewLastModified;

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

    killPlayerSafe();

    backup.begin(this);
    grayOut(&backup);
    backup.end();

    iconhelp->addLayout(grid);

    QButton *button = iconhelp->addButton(tr("Ok"));
    button->setFocus();

    iconhelp->ExecPopup();

    delete iconhelp;

    backup.begin(this);
    backup.drawPixmap(0, 0, myBackground);
    backup.end();

    state = kChanging;

    skipUpdate = false;
    skipCnt = 2;
    update(fullRect);

    setActiveWindow();
}

void PlaybackBox::showViewChanger(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();


    MythPopupBox tmpPopup(gContext->GetMainWindow(),
                                              true, popupForeground,
                                              popupBackground, popupHighlight,
                                              "change group view");
    choosePopup = &tmpPopup;

    QLabel *label = choosePopup->addLabel(tr("Change Group View"),
                                  MythPopupBox::Large, false);
    label->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    QStringList views;

    views += tr("Show Titles only");
    views += tr("Show Titles and Categories");
    views += tr("Show Titles, Categories, and Recording Groups");
    views += tr("Show Titles and Recording Groups");
    views += tr("Show Categories only");
    views += tr("Show Categories and Recording Groups");
    views += tr("Show Recording Groups only");

    chooseComboBox = new MythComboBox(false, choosePopup);
    chooseComboBox->insertStringList(views);
    chooseComboBox->setAcceptOnSelect(true);
    choosePopup->addWidget(chooseComboBox);

    connect(chooseComboBox, SIGNAL(accepted(int)), this,
            SLOT(chooseSetGroupView()));

    chooseComboBox->setFocus();
    choosePopup->ExecPopup();

    delete chooseComboBox;
    chooseComboBox = NULL;

    skipUpdate = false;
    skipCnt = 2;
    update(fullRect);

    setActiveWindow();
}

void PlaybackBox::chooseSetGroupView(void)
{
    if (!chooseComboBox)
        return;

    setDefaultView(chooseComboBox->currentItem());

    inTitle = gContext->GetNumSetting("PlaybackBoxStartInTitle", 0);
    titleIndex = 0;
    progIndex = 0;
    playList.clear();

    connected = FillList();
    skipUpdate = false;
    update(fullRect);
    choosePopup->done(0);
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

    timer->stop();
    playingVideo = false;

    backup.begin(this);
    grayOut(&backup);
    backup.end();

    QLabel *label = choosePopup->addLabel(tr("Select Group Filter"),
                                  MythPopupBox::Large, false);
    label->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    QStringList groups;
    
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT DISTINCT recgroup, COUNT(title) "
                  "FROM recorded GROUP BY recgroup;");
    query.exec();

    QString tmpType = recGroupType[recGroup];
    recGroupType.clear();

    recGroupType[recGroup] = tmpType;

    if ((recGroup == "Default") || (recGroup == "All Programs"))
        groups += tr(recGroup);

    if (recGroup != "All Programs")
    {
        groups += tr("All Programs");
        recGroupType["All Programs"] = "recgroup";
    }

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            QString key =
                QString::fromUtf8(QString("%1 [%2 items]")
                                  .arg(query.value(0).toString())
                                  .arg(query.value(1).toString()));

            if (key != recGroup)
            {
                if (key == "Default")
                    groups += tr("Default");
                else
                    groups += key;
                recGroupType[query.value(0).toString()] = "recgroup";
            }
        }
    }

    query.prepare("SELECT DISTINCT category, COUNT(title) "
                   "FROM recorded GROUP BY category;");

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            QString key =
                QString::fromUtf8(QString("%1 [%2 items]")
                                  .arg(query.value(0).toString())
                                  .arg(query.value(1).toString()));

            if ((key != recGroup) && (key != ""))
            {
                groups += key;
                if (!recGroupType.contains(key))
                    recGroupType[query.value(0).toString()] = "category";
            }
        }
    }

    chooseListBox = new MythListBox(choosePopup);
    chooseListBox->insertStringList(groups);

    choosePopup->addWidget(chooseListBox);

    connect(chooseListBox, SIGNAL(accepted(int)), this,
            SLOT(chooseSetViewGroup()));
    connect(chooseListBox, SIGNAL(highlighted(int)), this,
            SLOT(chooseListBoxChanged()));

    chooseGroupPassword = getRecGroupPassword(recGroup);

    chooseListBox->setFocus();
    choosePopup->ExecPopup();

    delete chooseListBox;
    chooseListBox = NULL;

    backup.begin(this);
    backup.drawPixmap(0, 0, myBackground);
    backup.end();

    skipUpdate = false;
    skipCnt = 2;
    update(fullRect);

    setActiveWindow();

    timer->start(500);
}

void PlaybackBox::chooseSetViewGroup(void)
{
    if (!chooseListBox)
        return;

    recGroupPassword = chooseGroupPassword;
    recGroup =
        chooseListBox->currentText().section('[', 0, 0).simplifyWhiteSpace();

    if (groupnameAsAllProg)
        groupDisplayName = recGroup;

    if (recGroup == tr("Default"))
        recGroup = "Default";
    if (recGroup == tr("All Programs"))
        recGroup = "All Programs";

    if (recGroupPassword != "" )
    {

        bool ok = false;
        QString text = tr("Password:");

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

    inTitle = gContext->GetNumSetting("PlaybackBoxStartInTitle", 0);
    titleIndex = 0;
    progIndex = 0;
    playList.clear();

    connected = FillList();
    skipUpdate = false;
    update(fullRect);
    choosePopup->done(0);
}

void PlaybackBox::chooseListBoxChanged(void)
{
    if (!chooseListBox)
        return;

    QString newGroup =
        chooseListBox->currentText().section('[', 0, 0).simplifyWhiteSpace();

    if (newGroup == tr("Default"))
        newGroup = "Default";
    if (newGroup == tr("All Programs"))
        newGroup = "All Programs";

    chooseGroupPassword = getRecGroupPassword(newGroup);
}

QString PlaybackBox::getRecGroupPassword(QString group)
{
    QString result = QString("");

    if (group == "All Programs")
    {
        result = gContext->GetSetting("AllRecGroupPassword");
    }
    else
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT password FROM recgrouppassword "
                                   "WHERE recgroup = :GROUP ;");
        query.bindValue(":GROUP", group);

        if (query.exec() && query.isActive() && query.size() > 0)
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

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT recgroup, password "
                  "FROM recgrouppassword "
                  "WHERE password is not null "
                  "AND password <> '' ;");

    if (query.exec() && query.isActive() && query.size() > 0)
        while (query.next())
            recGroupPwCache[query.value(0).toString()] =
                query.value(1).toString();
}

void PlaybackBox::doPlaylistChangeRecGroup(void)
{
    // This is needed to signal the RecGroup changer that we are taking
    // action on the entire playlist.  Since the same function can be
    // called with a playlist built but targeted against a single item
    // checking the existance of a playlist is not enough
    onPlaylist = true;
    showRecGroupChanger();
    onPlaylist = false;
}

void PlaybackBox::showRecGroupChanger(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    MythPopupBox tmpPopup(gContext->GetMainWindow(),
                                              true, popupForeground,
                                              popupBackground, popupHighlight,
                                              "change recgroup view");
    choosePopup = &tmpPopup;


    QLabel *label = choosePopup->addLabel(tr("Change Recording Group"),
                                  MythPopupBox::Large, false);
    label->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    QStringList groups;


    if (delitem->recgroup == "Default")
        groups += tr("Default");
    else
        groups += delitem->recgroup;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT DISTINCT recgroup from recorded");

    if (query.exec() && query.isActive() && query.size() > 0)
        while (query.next())
            if (query.value(0).toString() != delitem->recgroup)
                if (query.value(0).toString() == "Default")
                    groups += tr("Default");
                else
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

    if (delitem)
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

    if (newRecGroup == tr("Default"))
        newRecGroup = "Default";

    if (onPlaylist)
    {
        ProgramInfo *tmpItem;
        QStringList::Iterator it;

        for (it = playList.begin(); it != playList.end(); ++it )
        {
            tmpItem = findMatchingProg(*it);
            if (tmpItem)
                tmpItem->ApplyRecordRecGroupChange(newRecGroup);
        }
        playList.clear();
    } else {
        delitem->ApplyRecordRecGroupChange(newRecGroup);
    }
    
    inTitle = gContext->GetNumSetting("PlaybackBoxStartInTitle", 0);
    titleIndex = 0;
    progIndex = 0;

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

    timer->stop();
    playingVideo = false;

    backup.begin(this);
    grayOut(&backup);
    backup.end();

    QLabel *label = choosePopup->addLabel(tr("Change Recording Group Password"),
                                  MythPopupBox::Large, false);
    label->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    QGridLayout *grid = new QGridLayout(3, 2, (int)(10 * wmult));

    label = new QLabel(tr("Recording Group:"), choosePopup);
    label->setAlignment(Qt::WordBreak | Qt::AlignLeft);
    label->setBackgroundOrigin(ParentOrigin);
    label->setPaletteForegroundColor(popupForeground);
    grid->addWidget(label, 0, 0, Qt::AlignLeft);

    if ((recGroup == "Default") || (recGroup == "All Programs"))
        label = new QLabel(tr(recGroup), choosePopup);
    else
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

    backup.begin(this);
    backup.drawPixmap(0, 0, myBackground);
    backup.end();

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

    if (recGroup == "All Programs")
    {
        gContext->SaveSetting("AllRecGroupPassword", newPassword);
    }
    else
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("DELETE FROM recgrouppassword "
                           "WHERE recgroup = :RECGROUP ;");
        query.bindValue(":RECGROUP", recGroup);       

        query.exec();

        if (newPassword != "")
        {
            query.prepare("INSERT INTO recgrouppassword "
                          "(recgroup, password) VALUES "
                          "( :RECGROUP , :PASSWD )");
            query.bindValue(":RECGROUP", recGroup);
            query.bindValue(":PASSWD", newPassword);

            query.exec();
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
