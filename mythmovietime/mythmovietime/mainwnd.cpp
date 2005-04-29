/* ============================================================
 * File  : mainwnd.cpp
 * Author: J. Donavan Stanley <jdonavan@jdonavan.net>
 *
 * Abstract: This file includes the implementation for the main 
 *           window of the MythMovieTime plugin.
 *
 *
 * Copyright 2005 by J. Donavan Stanley
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#include <qapplication.h>
#include <qsqldatabase.h>
#include <qstringlist.h>
#include <qpixmap.h>
#include <qregexp.h>
#include <qframe.h>
#include <qlayout.h>
#include <qevent.h>

#include <mythtv/dialogbox.h>
#include <mythtv/mythcontext.h>
#include <mythtv/lcddevice.h>
#include <mythtv/uitypes.h>
#include <mythtv/uilistbtntype.h>

#include "mainwnd.h"
#include "mtdbitem.h"


#define LCD_MAX_MENU_ITEMS 5



MMTMainWindow::MMTMainWindow(MythMainWindow *parent, const QString &window_name, 
                             const QString &theme_filename, const char *name)
             : MythThemedDialog(parent, window_name, theme_filename, name)
{
    ActivePopup = NULL;

    Zoom = QString("-z %1").arg(gContext->GetNumSetting("WebBrowserZoomLevel",200));
    Browser = gContext->GetSetting("WebBrowserCommand",
                                   gContext->GetInstallPrefix() + "/bin/mythbrowser");

    Info = getContainer("info");
    Tree = getUIListTreeType("movietree");
        
    if (!Tree)
    {
        DialogBox diag(gContext->GetMainWindow(), 
                       tr("The theme you are using "
                          "does not contain a 'movietretree' element.  "
                          "Please contact the theme creator and ask if they could "
                          "please update it.<br><br>The next screen will be empty."
                          "  Escape out of it to return to the menu."));
        diag.AddButton(tr("OK"));
        diag.exec();

        return;
    }
    
    connect(Tree, SIGNAL(itemEntered(UIListTreeType *, UIListGenericTree *)),
            this, SLOT(entered(UIListTreeType *, UIListGenericTree *)));    
    
    initializeTree();
}




MMTMainWindow::~MMTMainWindow(void)
{

    delete RootNode;
}


void MMTMainWindow::entered(UIListTreeType *treetype, UIListGenericTree *item)
{
    if (!item || !treetype)
        return;
        
    
    MMTDBItem *pItem = dynamic_cast<MMTDBItem*>(item);

    if (pItem && item->childCount() == 0)
    {    
        pItem->populateTree(item);    
        Tree->Redraw();
    }        
    
    if (pItem )
    {
        QMap<QString, QString> map;
        
        pItem->toMap(map);
        UITextType* textObj;
        
        QMap<QString, QString>::Iterator it;
        for( it = map.begin(); it != map.end(); ++it )
        {
            textObj = getUITextType( it.key() );
            if( textObj )
            {
                textObj->SetText( it.data() );
            }
        }
    }        
}

void MMTMainWindow::selected(UIListGenericTree *item)
{
    if (!item)
        return;

//    UIListGenericTree *parent = (UIListGenericTree *)item->getParent(); 
}

void MMTMainWindow::initializeTree()
{
    // Make the first few nodes in the tree,
    RootNode = new UIListGenericTree(NULL, "Root Node");            
    
    MoviesRoot   = new MMTMovieMasterItem(RootNode, tr("Movies"));
    TheatersRoot = new MMTTheaterMasterItem(RootNode, tr("Theaters"));
    GenreRoot    = new MMTGenreMasterItem(RootNode, tr("Genre"));
    
    Tree->SetTree(RootNode);
}



void MMTMainWindow::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("movietime", e, actions);

    UIListGenericTree *curItem = Tree->GetCurrentPosition();

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "INFO")
            doMenu(curItem, true);
        else if (action == "SELECT")
            selected(curItem);
        else if (action == "UP")
            Tree->MoveUp();
        else if (action == "DOWN")
            Tree->MoveDown();
        else if (action == "LEFT")
            Tree->MoveLeft();
        else if (action == "RIGHT")
            Tree->MoveRight();
        else if (action == "PAGEUP")
            Tree->MoveUp(UIListTreeType::MovePage);
        else if (action == "PAGEDOWN")
            Tree->MoveDown(UIListTreeType::MovePage);
        else
            handled = false;
    }

    if (handled)
        return;

    MythDialog::keyPressEvent(e);
}

void MMTMainWindow::doMenu(UIListGenericTree* item, bool infoMenu)
{
    int buttonCount = 0;
    
    MMTDBItem *curItem = dynamic_cast<MMTDBItem*>(item);
    
    if (ActivePopup || !curItem)
    {
        return;
    }
    
    ActivePopup = new MythPopupBox(gContext->GetMainWindow(), "popup");
    if (infoMenu)
    {
        if (curItem->getFilmID() > 0 )
        {
            buttonCount++;
            ActivePopup->addButton(tr("Full Film Details"), this, SLOT(doFullFilmInfo()));
        }
        
        if (!curItem->getOfficialURL().isEmpty() )
        {
            buttonCount++;
            ActivePopup->addButton(tr("Visit Movie Website"), this, SLOT(doVisitSite()));
        }

        MMTShowTimeItem *showItem = dynamic_cast<MMTShowTimeItem*>(item);
        if (showItem)
        {
            
            if (!showItem->getTicketURL().isEmpty() )
            {
                buttonCount++;
                ActivePopup->addButton(tr("Buy Ticket"), this, SLOT(doBuyTicket()));
            }
        }
        
        if (buttonCount)              
        {
            ActivePopup->ShowPopup(this, SLOT(closeActivePopup()));
        }
        else
        {
            delete ActivePopup;
            ActivePopup = NULL;
        }
    }
}

void MMTMainWindow::closeActivePopup(void)
{
    if (!ActivePopup)
        return;

    ActivePopup->hide();
    delete ActivePopup;
    ActivePopup = NULL;
}

void MMTMainWindow::launchBrowser(const QString& url) const
{
    QString cmdUrl = url;
    
    // Sanity check
    if (cmdUrl.isEmpty() )
    {
        VERBOSE( VB_IMPORTANT, "MMTMainWindow::launchBrowser no URL given");
        return;
    }
    
    cmdUrl.replace('\'', "%27");
    
    QString cmd = QString("%1 %2 '%3'")
                         .arg(Browser)
                         .arg(Zoom)
                         .arg(cmdUrl);
    
    myth_system(cmd);
}

void MMTMainWindow::doVisitSite()
{
    UIListGenericTree *curItem = Tree->GetCurrentPosition();
    MMTDBItem *pItem = dynamic_cast<MMTDBItem*>(curItem);
    
    closeActivePopup();
    
    if (pItem)
    {
        launchBrowser(pItem->getOfficialURL());
    }

}

void MMTMainWindow::doBuyTicket()
{
    UIListGenericTree *curItem = Tree->GetCurrentPosition();
    MMTShowTimeItem *pItem = dynamic_cast<MMTShowTimeItem*>(curItem);
    closeActivePopup();
    if (pItem)
    {
        launchBrowser(pItem->getTicketURL());
    }
}

void MMTMainWindow::doFullFilmInfo()
{
    closeActivePopup();
}

// EOF

