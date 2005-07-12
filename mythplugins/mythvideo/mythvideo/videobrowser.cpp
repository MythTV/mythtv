#include <qlayout.h>
#include <qapplication.h>
#include <qcursor.h>
#include <qstringlist.h>
#include <qpixmap.h>
#include <iostream>
#include <unistd.h>
#include <qregexp.h>
#include <qnetwork.h>
#include <qurl.h>
#include <qdir.h>

using namespace std;

#include "metadata.h"
#include "videobrowser.h"
#include <mythtv/mythcontext.h>
#include <mythtv/util.h>

#include "videofilter.h"



VideoBrowser::VideoBrowser(MythMainWindow *parent, const char *name)
            : VideoDialog(DLG_BROWSER, parent, "browser", name)
{
    m_state = 0;
    
    setFileBrowser(gContext->GetNumSetting("VideoBrowserNoDB", 0));
    loadWindow(xmldata);        
    bgTransBackup = gContext->LoadScalePixmap("trans-backup.png");
    
    if (!bgTransBackup)
        bgTransBackup = new QPixmap();

    setNoErase();

    fetchVideos();
    updateBackground();
}

VideoBrowser::~VideoBrowser()
{
    delete bgTransBackup;
}


void VideoBrowser::slotParentalLevelChanged()
{
    LayerSet *container = theme->GetSet("browsing");
    if(container)
    {
        UITextType *pl_value = (UITextType *)container->GetType("pl_value");
        if (pl_value)
           pl_value->SetText(QString("%1").arg(currentParentalLevel));
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
            jumpSelection(video_list->count() / 5);
        else if (action == "PAGEUP")
            jumpSelection(-(video_list->count() / 5));
        else if (action == "LEFT")
            cursorLeft();
        else if (action == "RIGHT")
            cursorRight();
        else if (action == "HOME")
            jumpToSelection(0);
        else if (action == "END")
            jumpToSelection(video_list->count() - 1);
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
        if(info)
        {
            focusButton = popup->addButton(tr("Watch This Video"), this, SLOT(slotWatchVideo())); 
            popup->addButton(tr("View Full Plot"), this, SLOT(slotViewPlot()));
        }
        else
        {
            if (!isFileBrowser)
                focusButton = popup->addButton(tr("Filter Display"), this, SLOT(slotDoFilter()));
            
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
    video_list->wantVideoListUpdirs(isFileBrowser);
    
    SetCurrentItem(0);
    update(infoRect);
    update(browsingRect);
    repaint();
}

void VideoBrowser::grayOut(QPainter *tmp)
{
   int transparentFlag = gContext->GetNumSetting("PlayBoxShading", 0);
   if (transparentFlag == 0)
       tmp->fillRect(QRect(QPoint(0, 0), size()), QBrush(QColor(10, 10, 10), Dense4Pattern));
   else if (transparentFlag == 1)
       tmp->drawPixmap(0, 0, *bgTransBackup, 0, 0, (int)(800*wmult), (int)(600*hmult));
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
        container->Draw(p, 0, 0);
        container->Draw(p, 1, 0);
        container->Draw(p, 2, 0);
        container->Draw(p, 3, 0);
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
    
    unsigned int list_count = video_list->count();
    
    if (list_count == 0)
        return;

    inData = QMIN(list_count - 1, index);
    curitem = video_list->getVideoListMetadata(inData);
}

void VideoBrowser::updateBrowsing(QPainter *p)
{
    QRect pr = browsingRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    unsigned int list_count = video_list->count();

    QString vidnum;  
    if (list_count > 0)
        vidnum = QString(tr("%1 of %2")).arg(inData + 1).arg(list_count);
    else
        vidnum = tr("No Videos");

    LayerSet *container = NULL;
    container = theme->GetSet("browsing");
    if (container)
    {
        UITextType *type = (UITextType *)container->GetType("currentvideo");
        if (type)
            type->SetText(vidnum);

        UITextType *pl_value = (UITextType *)container->GetType("pl_value");
        if (pl_value)
        {
           pl_value->SetText(QString("%1").arg(currentParentalLevel));
        }

        container->Draw(&tmp, 1, 0);
        container->Draw(&tmp, 2, 0);
        container->Draw(&tmp, 3, 0);
        container->Draw(&tmp, 4, 0);
        container->Draw(&tmp, 5, 0);
        container->Draw(&tmp, 6, 0);
        container->Draw(&tmp, 7, 0);
        container->Draw(&tmp, 8, 0);
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

    if (video_list->count() > 0 && curitem)
    {
       QString title = curitem->Title();
       QString filename = curitem->Filename();
       QString director = curitem->Director();
       QString year = QString("%1").arg(curitem->Year());
       if (year == "1895") 
           year = "?";
       QString coverfile = curitem->CoverFile();
       QString inetref = curitem->InetRef();
       QString plot = curitem->Plot();
       QString userrating = QString("%1").arg(curitem->UserRating());
       QString rating = curitem->Rating();
       if (rating == "<NULL>")
           rating = tr("No rating available.");
       QString length = QString("%1").arg(curitem->Length()) + " " + tr("minutes");
       QString level = QString("%1").arg(curitem->ShowLevel());

       LayerSet *container = NULL;
       container = theme->GetSet("info");
       if (container)
       {
           UITextType *type = (UITextType *)container->GetType("title");
           if (type)
               type->SetText(title);

           type = (UITextType *)container->GetType("filename");
           if (type)
               type->SetText(filename);

           type = (UITextType *)container->GetType("director");
           if (type)
               type->SetText(director);
 
           type = (UITextType *)container->GetType("year");
           if (type)
               type->SetText(year);

           type = (UITextType *)container->GetType("coverfile");
           if (type)
               type->SetText(coverfile);
  
           UIImageType *itype = (UIImageType *)container->GetType("coverart");
           if (itype && (coverfile != QObject::tr("No Cover")) && (coverfile != QObject::tr("None")))
           {
               if (itype->GetImageFilename() != coverfile)
               {
                   itype->SetImage(coverfile);
                   itype->LoadImage();
               }
               if (itype->isHidden())
                   itype->show();   
           }
           else
           {
               if (itype->isShown())
                   itype->hide();   
           }

           type = (UITextType *)container->GetType("inetref");
           if (type)
               type->SetText(inetref);

           type = (UITextType *)container->GetType("plot");
           if (type)
               type->SetText(plot);
 
           type = (UITextType *)container->GetType("userrating");
           if (type)
               type->SetText(userrating);

           type = (UITextType *)container->GetType("rating");
           if (type)
               type->SetText(rating);

           type = (UITextType *)container->GetType("length");
           if (type)
               type->SetText(length);

           type = (UITextType *)container->GetType("level");
           if (type)
               type->SetText(level);
  
           container->Draw(&tmp, 1, 0); 
           container->Draw(&tmp, 2, 0);
           container->Draw(&tmp, 3, 0);
           container->Draw(&tmp, 4, 0);  
           container->Draw(&tmp, 5, 0);
           container->Draw(&tmp, 6, 0); 
           container->Draw(&tmp, 7, 0);
           container->Draw(&tmp, 8, 0);
       }
    }
    else
    {
       LayerSet *norec = theme->GetSet("novideos_info");
       if (norec)
       {
           norec->Draw(&tmp, 4, 0);
           norec->Draw(&tmp, 5, 0);
           norec->Draw(&tmp, 6, 0);
           norec->Draw(&tmp, 7, 0);
           norec->Draw(&tmp, 8, 0);
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
    unsigned int list_count = video_list->count();
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
    QString name;
    int context;
    theme->parseContainer(element, name, context, area);
    if (name.lower() == "info")
        infoRect = area;
    if (name.lower() == "browsing")
        browsingRect = area;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
