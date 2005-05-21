/*
	themeddialog.cpp

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include <iostream>
using namespace std;

#include <qdict.h>

#include "myththemeddialog.h"
#include "mythfontproperties.h"
#include "mythuitext.h"
#include "mythmainwindow.h"
#include "mythxmlparser.h"
#include "mythuicontainer.h"
#include "mythuitree.h"

#include "myththemeddialogprivate.h"



MythUIThemedDialog::MythUIThemedDialog(MythScreenStack *parent, 
                                       const QString &screen_name,
                                       const QString &theme_file,
                                       const QString &theme_dir) 
         : MythScreenType(parent, screen_name)
{

    d = new MythUIThemedDialogPrivate(this, 
                                      screen_name, 
                                      theme_file, 
                                      theme_dir);
}

bool MythUIThemedDialog::keyPressEvent(QKeyEvent *e)
{
    
    bool handled = false;
    QStringList actions;
    if (GetMythMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "ESCAPE")
            {
                m_ScreenStack->PopScreen();
            }
            else
            {
                handled = false;
            }
        }
    }

    return handled;
}

MythUITree* MythUIThemedDialog::getMythUITree(const QString &widget_name)
{
    return d->getMythUITree(widget_name);
}

MythUIThemedDialog::~MythUIThemedDialog()
{
    if(d)
    {
        delete d;
        d = NULL;
    }
}

