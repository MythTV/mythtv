/*
Copyright (c) 2004 Koninklijke Philips Electronics NV. All rights reserved.
Based on "videobrowser.cpp" of MythVideo.

This is free software; you can redistribute it and/or modify it under the
terms of version 2 of the GNU General Public License as published by the
Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include <qlayout.h>
#include <qapplication.h>
#include <qsqldatabase.h>
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
#include "videoselected.h"
#include <mythtv/mythcontext.h>
#include <mythtv/util.h>
#include <mythtv/mythdbcon.h>


VideoSelected::VideoSelected(QSqlDatabase *ldb,
                             MythMainWindow *parent, const char *name, 
                             int idnum)
            : MythDialog(parent, name)
{
    db = ldb;

    curitem = new Metadata();
    curitem->setID(idnum);
    curitem->fillDataFromID(db);

    noUpdate = false;
    m_state = 0;

    
    fullRect = QRect(0, 0, size().width(), size().height());
    
    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    theme->LoadTheme(xmldata, "selected", "video-");
    LoadWindow(xmldata);

    bgTransBackup = gContext->LoadScalePixmap("trans-backup.png");
    if (!bgTransBackup)
        bgTransBackup = new QPixmap();

    updateBackground();

    setNoErase();
}

VideoSelected::~VideoSelected()
{
    delete theme;
    delete bgTransBackup;
    if (curitem)
        delete curitem;
}

void VideoSelected::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Video", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];


        if (action == "SELECT" && allowselect)
        {
            handled = true;
            selected(curitem);
            return;

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
                    selected(curitem);
                }
            }            
        }
    }
    
    if (!handled)
        MythDialog::keyPressEvent(e);
}

void VideoSelected::updateBackground(void)
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

void VideoSelected::grayOut(QPainter *tmp)
{
   int transparentFlag = gContext->GetNumSetting("PlayBoxShading", 0);
   if (transparentFlag == 0)
       tmp->fillRect(QRect(QPoint(0, 0), size()), QBrush(QColor(10, 10, 10), Dense4Pattern));
   else if (transparentFlag == 1)
       tmp->drawPixmap(0, 0, *bgTransBackup, 0, 0, (int)(800*wmult), (int)(600*hmult));
}

void VideoSelected::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);

    if (m_state == 0)
    {
       if (r.intersects(infoRect) && noUpdate == false)
       {
           updateInfo(&p);
       }
    }
    else if (m_state > 0)
    {
        noUpdate = true;
        updatePlayWait(&p);
    }
}

void VideoSelected::updatePlayWait(QPainter *p)
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
    QTime playing_time;
    playing_time.start();

    // Play the movie
    myth_system((QString("%1 ") .arg(m_cmd)).local8Bit());

    Metadata *childItem = new Metadata;
    Metadata *parentItem = new Metadata(*curitem);

    while (parentItem->ChildID() > 0 && playing_time.elapsed() > 10000)
    {
        childItem->setID(parentItem->ChildID());
        childItem->fillDataFromID(db);

        if (parentItem->ChildID() > 0)
        {
            //Load up data about this child
            selected(childItem);
            playing_time.start();
            myth_system((QString("%1 ") .arg(m_cmd)).local8Bit());
        }

        delete parentItem;
        parentItem = new Metadata(*childItem);
    }

    delete childItem;
    delete parentItem;

    backup.begin(this);
    backup.drawPixmap(0, 0, myBackground);
    backup.end();
    noUpdate = false;

    gContext->GetMainWindow()->raise();
    gContext->GetMainWindow()->setActiveWindow();
    gContext->GetMainWindow()->currentWidget()->setFocus();

    m_state = 0;
    update(fullRect);
  }
}

void VideoSelected::updateInfo(QPainter *p)
{
    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    if (curitem)
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
       QString length = QString("%1").arg(curitem->Length()) + " " +
	      	      	tr("minutes");
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
           if (itype)
           {
               itype->SetImage(coverfile);
               itype->LoadImage();
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

       allowselect = true;
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

       allowselect = false;
    }
    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);

}

void VideoSelected::LoadWindow(QDomElement &element)
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
            else
            {
                cerr << "Unknown element: " << e.tagName() << endl;
                exit(0);
            }
        }
    }
}

void VideoSelected::parseContainer(QDomElement &element)
{
    QRect area;
    QString name;
    int context;
    theme->parseContainer(element, name, context, area);

    if (name.lower() == "info")
        infoRect = area;
}
 
void VideoSelected::exitWin()
{
    emit accept();
}

void VideoSelected::selected(Metadata *someItem)
{
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
    
    QString year = QString("%1").arg(someItem->Year());
    // See if this is being handled by a plugin..
    if(gContext->GetMainWindow()->HandleMedia(handler, filename, someItem->Plot(), 
                                              someItem->Title(), someItem->Director(),
                                                  someItem->Length(), year))
    {
        return;
    }
    
 
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
    if(handler.contains("%d"))
    {
	QString default_handler = gContext->GetSetting("VideoDefaultPlayer");
	if(handler.contains("%s") && default_handler.contains("%s"))
	{
		default_handler = default_handler.replace(QRegExp("%s"), "");
	}
	command = handler.replace(QRegExp("%d"), default_handler);
    }

    if(handler.contains("%s"))
    {
        command = handler.replace(QRegExp("%s"), arg);
    }
    else
    {
        command = handler + " " + arg;
    }

    //cout << "command:" << command << endl;

    m_title = someItem->Title();
    LayerSet *container = NULL;
    container = theme->GetSet("playwait");
    if (container)
    {
         UITextType *type = (UITextType *)container->GetType("title");
         if (type)
             type->SetText(m_title);
    }
    m_cmd = command;
    m_state = 1;
    update(fullRect);
}
