#include "videodlg.h"

#include <mythtv/xmlparse.h>
#include <mythtv/mythcontext.h>
#include <mythtv/util.h>
#include <mythtv/mythdbcon.h>


#include "videofilter.h"


const long WATCHED_WATERMARK = 10000; // Less than this and the chain of videos will 
                                      // not be followed when playing.

VideoDialog::VideoDialog(DialogType _myType, QSqlDatabase *_db, 
                         MythMainWindow *_parent,  const char* _winName, const char *_name)
           : MythDialog(_parent, _name)
{
    db = _db;
    myType = _myType;
    curitem = NULL;    
    popup = NULL;
    
    //
    //  Load the theme. Crap out if we can't find it.
    //

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    if (!theme->LoadTheme(xmldata, _winName, "video-"))
    {
        cerr << "VideoDialog: Couldn't find your theme. I'm outta here" << endl;
        cerr << _winName << " - " <<  "video-ui" << endl;
        exit(0);
    }

    expectingPopup = false;
    fullRect = QRect(0, 0, size().width(), size().height());
    allowPaint = true;
    currentParentalLevel = gContext->GetNumSetting("VideoDefaultParentalLevel", 1);
    currentVideoFilter = new VideoFilterSettings(db, true, _winName);
}

VideoDialog::~VideoDialog()
{
    if (currentVideoFilter)
        delete currentVideoFilter;
    
}

void VideoDialog::slotVideoBrowser()
{
    cancelPopup();
    gContext->GetMainWindow()->JumpTo("Video Browser");
}

void VideoDialog::slotVideoGallery()
{
    cancelPopup();
    gContext->GetMainWindow()->JumpTo("Video Gallery");
}

void VideoDialog::slotVideoTree()
{
    cancelPopup();
    gContext->GetMainWindow()->JumpTo("Video Listings");
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
        popup->hide();
        delete popup;

        popup = NULL;

        update(fullRect);
        qApp->processEvents();
        setActiveWindow();
    }
}

QButton* VideoDialog::addDests(MythPopupBox* _popup)
{
    if (!_popup)
        _popup = popup;
        
    if (!_popup)
        return NULL;
    
    QButton *focusButton = NULL;
    QButton *tempButton = NULL;
        
    if (myType != DLG_BROWSER)        
        focusButton = popup->addButton(tr("Switch to Browse View"), this, SLOT(slotVideoBrowser()));  
    
    if (myType != DLG_GALLERY)        
        tempButton = popup->addButton(tr("Switch to Gallery View"), this, SLOT(slotVideoGallery()));  
    
    focusButton = focusButton ? focusButton : tempButton;
    
    
    if (myType != DLG_TREE)
        tempButton = popup->addButton(tr("Switch to List View"), this, SLOT(slotVideoTree()));
    
    focusButton = focusButton ? focusButton : tempButton;        
    
    return focusButton;
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
        
        QLabel *plotLabel = plotbox->addLabel(curitem->Plot(),MythPopupBox::Small,true);
        plotLabel->setAlignment(Qt::AlignJustify | Qt::WordBreak);
        
        QButton * okButton = plotbox->addButton(tr("Ok"));
        okButton->setFocus();
        
        plotbox->ExecPopup();
        delete plotbox;
        allowPaint = true;
    }
    else
    {
        cerr << "no Item to view" << endl;
    }
}


void VideoDialog::slotWatchVideo()
{
    cancelPopup();
    
    if (curitem)
        playVideo(curitem);
    else
        cerr << "no Item to watch" << endl;

}


void VideoDialog::playVideo(Metadata *someItem)
{
    QString filename = someItem->Filename();
    QString handler = getHandler(someItem);
    QString year = QString("%1").arg(someItem->Year());
    // See if this is being handled by a plugin..
    if(gContext->GetMainWindow()->HandleMedia(handler, filename, someItem->Plot(), 
                                              someItem->Title(), someItem->Director(),
                                              someItem->Length(), year))
    {
        return;
    }

    QString command = getCommand(someItem);
        
    
    QTime playing_time;
    playing_time.start();
    
    

    // Show a please wait message    
    LayerSet *container = theme->GetSet("playwait");
    
    if (container)
    {
         UITextType *type = (UITextType *)container->GetType("title");
         if (type)
             type->SetText(someItem->Title());
    }
    update(fullRect);
    allowPaint = false;        
    // Play the movie
    myth_system((QString("%1 ").arg(command)).local8Bit());
    
    Metadata *childItem = new Metadata;
    Metadata *parentItem = new Metadata(*someItem);

    while (parentItem->ChildID() > 0 && playing_time.elapsed() > WATCHED_WATERMARK)
    {
        childItem->setID(parentItem->ChildID());
        childItem->fillDataFromID(db);

        if (parentItem->ChildID() > 0)
        {
            //Load up data about this child
            command = getCommand(childItem);
            playing_time.start();
            myth_system((QString("%1 ") .arg(command)).local8Bit());
        }

        delete parentItem;
        parentItem = new Metadata(*childItem);
    }

    delete childItem;
    delete parentItem;
    
    gContext->GetMainWindow()->raise();
    gContext->GetMainWindow()->setActiveWindow();
    gContext->GetMainWindow()->currentWidget()->setFocus();
    
    allowPaint = true;
    
    update(fullRect);
}


QString VideoDialog::getHandler(Metadata *someItem)
{
    
    if (!someItem)
        return "";
        
    QString filename = someItem->Filename();
    QString ext = someItem->Filename().section('.',-1);

    QString handler = gContext->GetSetting("VideoDefaultPlayer");
    QString special_handler = someItem->PlayCommand();
 
    //
    //  Does this specific metadata have its own
    //  unique player command?
    //
    if(special_handler.length() > 1)
    {
        handler = special_handler;
    }
    
    else
    {
        //
        //  Do we have a specialized player for this
        //  type of file?
        //
        
        QString extension = filename.section(".", -1, -1);

        MSqlQuery query(QString::null, db);
        query.prepare("SELECT playcommand, use_default FROM "
                      "videotypes WHERE extension = :EXT ;");
        query.bindValue(":EXT", extension);

        if(query.exec() && query.isActive() && query.size() > 0)
        {
            query.next();
            if(!query.value(1).toBool())
            {
                //
                //  This file type is defined and
                //  it is not set to use default player
                //

                handler = query.value(0).toString();                
            }
        }
    }
    
    return handler;
}

QString VideoDialog::getCommand(Metadata *someItem)
{
    if (!someItem)
        return "";
        
    QString filename = someItem->Filename();
    QString handler = getHandler(someItem);
    QString arg;
    arg.sprintf("\"%s\"",
                filename.replace(QRegExp("\""), "\\\"").utf8().data());

    QString command = "";
    
    // If handler contains %d, substitute default player command
    // This would be used to add additional switches to the default without
    // needing to retype the whole default command.  But, if the
    // command and the default command both contain %s, drop the %s from
    // the default since the new command already has it
    //
    // example: default: mplayer -fs %s
    //          custom : %d -ao alsa9:spdif %s
    //          result : mplayer -fs -ao alsa9:spdif %s
    if (handler.contains("%d"))
    {
        QString default_handler = gContext->GetSetting("VideoDefaultPlayer");
        if (handler.contains("%s") && default_handler.contains("%s"))
        {
            default_handler = default_handler.replace(QRegExp("%s"), "");
        }
        command = handler.replace(QRegExp("%d"), default_handler);
    }

    if (handler.contains("%s"))
    {
        command = handler.replace(QRegExp("%s"), arg);
    }
    else
    {
        command = handler + " " + arg;
    }
    
    return command;
}


void VideoDialog::setParentalLevel(int which_level)
{
    if (which_level < 1)
        which_level = 1;
    
    if (which_level > 4)
        which_level = 4;
    
    if ((which_level > currentParentalLevel) && !checkParentPassword())
        which_level = currentParentalLevel;
    
    
    if (currentParentalLevel != which_level)
    {
        currentParentalLevel = which_level;
        fetchVideos();
        slotParentalLevelChanged();
    }
}

bool VideoDialog::checkParentPassword()
{
    QDateTime curr_time = QDateTime::currentDateTime();
    QString last_time_stamp = gContext->GetSetting("VideoPasswordTime");
    QString password = gContext->GetSetting("VideoAdminPassword");
    if(password.length() < 1)
    {
        return true;
    }
    //
    //  See if we recently (and succesfully)
    //  asked for a password
    //
    
    if(last_time_stamp.length() < 1)
    {
        //
        //  Probably first time used
        //

        cerr << "videotree.o: Could not read password/pin time stamp. "
             << "This is only an issue if it happens repeatedly. " << endl;
    }
    else
    {
        QDateTime last_time = QDateTime::fromString(last_time_stamp, Qt::TextDate);
        if(last_time.secsTo(curr_time) < 120)
        {
            //
            //  Two minute window
            //
            last_time_stamp = curr_time.toString(Qt::TextDate);
            gContext->SetSetting("VideoPasswordTime", last_time_stamp);
            gContext->SaveSetting("VideoPasswordTime", last_time_stamp);
            return true;
        }
    }
    
    //
    //  See if there is a password set
    //
    
    if(password.length() > 0)
    {
        bool ok = false;
        MythPasswordDialog *pwd = new MythPasswordDialog(tr("Parental Pin:"),
                                                         &ok,
                                                         password,
                                                         gContext->GetMainWindow());
        pwd->exec();
        delete pwd;
        if(ok)
        {
            //
            //  All is good
            //

            last_time_stamp = curr_time.toString(Qt::TextDate);
            gContext->SetSetting("VideoPasswordTime", last_time_stamp);
            gContext->SaveSetting("VideoPasswordTime", last_time_stamp);
            return true;
        }
    }   
    else
    {
        return true;
    } 
    return false;
}


void VideoDialog::shiftParental(int amount)
{
    setParentalLevel(currentParentalLevel + amount);
}


void VideoDialog::fetchVideos()
{
    
    QString thequery = QString("SELECT intid FROM %1 %2 %3")
                        .arg(currentVideoFilter->BuildClauseFrom())
                        .arg(currentVideoFilter->BuildClauseWhere())
                        .arg(currentVideoFilter->BuildClauseOrderBy());

    MSqlQuery query(thequery,db);
    
    Metadata *myData;

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            myData = new Metadata();
            
            myData->setID(query.value(0).toUInt());
            myData->fillDataFromID(db);
            if (myData->ShowLevel() <= currentParentalLevel && myData->ShowLevel() != 0)
            {
                handleMetaFetch(myData);
            }
        
            delete myData;
        }
    }

}

void VideoDialog::slotDoFilter()
{
    cancelPopup();
    VideoFilterDialog * vfd = new VideoFilterDialog(db, currentVideoFilter,
                                                    gContext->GetMainWindow(),
                                                    "filter", "video-",
                                                    "Video Filter Dialog");
    vfd->exec();
    delete vfd;

    fetchVideos();
}

void VideoDialog::exitWin()
{
    emit accept();
}


void VideoDialog::loadWindow(QDomElement &element)
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
                this->parseContainer(e);
            }
            else
            {
                MythPopupBox::showOkPopup(gContext->GetMainWindow(), "",
                                          tr(QString("There is a problem with your"
                                          "music-ui.xml file... Unknown element: %1").
                                          arg(e.tagName())));
                
                cerr << "Unknown element: " << e.tagName() << endl;
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
