/* ============================================================
 * File  : mainwnd.h
 * Author: J. Donavan Stanley <jdonavan@jdonavan.net>
 *
 * Abstract: This file includes the definition for the main 
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
#ifndef _MMT_MAIN_WND
#define _MMT_MAIN_WND

#include <qwidget.h>
#include <qdialog.h>
#include <qstringlist.h>
#include <qthread.h>
#include <qtimer.h>
#include <qptrlist.h>
 
#include <mythtv/mythwidgets.h>
#include <mythtv/lcddevice.h>
#include <mythtv/uilistbtntype.h>


class MMTDBItem;


class MMTMainWindow : public MythThemedDialog
{
    Q_OBJECT

    public:

        MMTMainWindow( MythMainWindow *parent, const QString &window_name, 
                       const QString &theme_filename, const char *name = 0);
        ~MMTMainWindow(void);

    protected slots:
        void selected(UIListGenericTree *);
        void entered(UIListTreeType *, UIListGenericTree *);
        void keyPressEvent(QKeyEvent *e);
        
    protected:    
        void initializeTree();
        void doMenu(UIListGenericTree*);
        
        //  Theme widgets
        UIListGenericTree   *RootNode;
        UIListTreeType      *Tree;
        LayerSet            *Info;
        
        
        MMTDBItem* MoviesRoot;
        MMTDBItem* TheatersRoot;
        
};








#endif
