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
 * Public License as published bythe Free Software Foundation;
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
}

void MMTMainWindow::selected(UIListGenericTree *item)
{
    if (!item)
        return;

    UIListGenericTree *parent = (UIListGenericTree *)item->getParent(); 
}

void MMTMainWindow::initializeTree()
{
    // Make the first few nodes in the tree,
    RootNode = new UIListGenericTree(NULL, "Root Node");            
    
    MoviesRoot = new MMTDBItem(RootNode, tr("Movies"), KEY_MOVIE, 0);
    TheatersRoot = new MMTDBItem(RootNode, tr("Theaters"), KEY_THEATER, 0);
    
    
    
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
            doMenu(curItem);
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

void MMTMainWindow::doMenu(UIListGenericTree*)
{

}


// EOF

