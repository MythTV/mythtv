/*
	fileassoc.cpp

	(c) 2003 Thor Sigvaldason, Isaac Richards, and ?? ??
	Part of the mythTV project
	
    Classes to manipulate the file associations stored
    in the videotypes table (in the mythconverg database)

*/

#include <qaccel.h>

#include "fileassoc.h"

FileAssociation::FileAssociation(QSqlDatabase *ldb, const QString &new_extension)
{
    //
    //  Create a new file association
    //  (empty)
    //

    if(!ldb)
    {
        cerr << "fileassoc.o: Not going to get very vary without a db pointer!" << endl;
        exit(0);
    }
    db = ldb;
    loaded_from_db = false;
    changed = true;
    id = -1;
    extension = new_extension;
    player_command = "";
    ignore = false;
    use_default = true;
}

FileAssociation::FileAssociation(QSqlDatabase *ldb,
                                 int   i,
                                 const QString &e,
                                 const QString &p,
                                 bool  g,
                                 bool  u)
{

    //
    // "Create" a file association that
    //  already exists in the db
    //

    if(!ldb)
    {
        cerr << "fileassoc.o: Not going to get very vary without a db pointer!" << endl;
        exit(0);
    }
    db = ldb;
    loaded_from_db = true;
    changed = false;

    id = i;
    extension = e;
    player_command = p;
    ignore = g;
    use_default = u;    

};

void FileAssociation::saveYourself()
{
    if(changed)
    {
        if(loaded_from_db)
        {
            //
            //  This one existed before, just need to
            //  save its changes.
            //
            
            QString q_string = QString( "UPDATE videotypes "
                                        "set playcommand = \"%1\", "
                                        "f_ignore = %2, "
                                        "use_default = %3 "
                                        "WHERE intid = %4 ;")
                                      .arg(player_command)
                                      .arg(ignore)
                                      .arg(use_default)
                                      .arg(id);
            QSqlQuery a_query(q_string, db);
            if(!a_query.isActive() || !a_query.numRowsAffected() > 0)
            {
                if(!a_query.isActive())
                {
                    cerr << "fileassoc.o: problem saving file association with this SQL: " << q_string << endl;
                }
            }
        }
        else
        {
            QString q_string = QString( "INSERT INTO videotypes "
                                        "(extension, playcommand, f_ignore, use_default) "
                                        "VALUES "
                                        "(\"%1\", \"%2\", %3, %4) ;")
                                      .arg(extension)
                                      .arg(player_command)
                                      .arg(ignore)
                                      .arg(use_default);
            QSqlQuery a_query(q_string, db);
            if(!a_query.isActive() || !a_query.numRowsAffected() > 0)
            {
                cerr << "fileassoc.o: problem creating file association with this SQL: " << q_string << endl;
            }
            
        }
    }
}

void FileAssociation::deleteYourselfFromDB()
{
    if(loaded_from_db)
    {
        QString q_string = QString("DELETE FROM videotypes WHERE intid = %1 ;")
                                  .arg(id);
        QSqlQuery a_query(q_string, db);
        if(!a_query.isActive() || !a_query.numRowsAffected() > 0)
        {
            cerr << "fileassoc.o: problem deleting file association with this SQL: " << q_string << endl;
        }
    }
}

/*
---------------------------------------------------------------------
*/

FileAssocDialog::FileAssocDialog(QSqlDatabase *ldb,
                                 MythMainWindow *parent, 
                                 QString window_name,
                                 QString theme_filename,
                                 const char* name)
                :MythThemedDialog(parent, window_name, theme_filename, name)
{
    if(!ldb)
    {
        cerr << "fileassoc.o: Where I am supposed to load stuff from if you don't give me a db pointer?" << endl;
        exit(0);
    }
    db = ldb;
    command_editor = NULL;
    file_associations.clear();
    file_associations.setAutoDelete(true);
    current_fa = NULL;
    new_extension_popup = NULL;
    new_extension_editor = NULL;
    wireUpTheme();
    assignFirstFocus();
    
    loadFileAssociations();
    showCurrentFA();
}

void FileAssocDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    
    switch (e->key())
    {

        //
        //  Widget Navigation
        //
        
        case Key_Up:
            nextPrevWidgetFocus(false);
            while(getCurrentFocusWidget()->GetContext() < -1)
            {
                nextPrevWidgetFocus(false);
            }
            handled = true;
            break;
        case Key_Down:
            nextPrevWidgetFocus(true);
            while(getCurrentFocusWidget()->GetContext() < -1)
            {
                nextPrevWidgetFocus(true);
            }
            handled = true;
            break;
        case Key_Left:
            if(extension_select)
            {
                if(extension_select == getCurrentFocusWidget())
                {
                    extension_select->push(false);
                }
            }
            if(default_check)
            {
                if(default_check == getCurrentFocusWidget())
                {
                    activateCurrent();
                }
            }
            if(ignore_check)
            {
                if(ignore_check == getCurrentFocusWidget())
                {
                    activateCurrent();
                }
            }
            handled = true;
            break;
        case Key_Right:
            if(extension_select)
            {
                if(extension_select == getCurrentFocusWidget())
                {
                    extension_select->push(true);
                }
            }
            if(default_check)
            {
                if(default_check == getCurrentFocusWidget())
                {
                    activateCurrent();
                }
            }
            if(ignore_check)
            {
                if(ignore_check == getCurrentFocusWidget())
                {
                    activateCurrent();
                }
            }
            handled = true;
            break;

        //
        //  Widget Activation
        //
        
        case Key_Space:
        case Key_Enter:
        case Key_Return:
            activateCurrent();
            handled = true;
            break;
    }
    if(!handled)
    {
        MythThemedDialog::keyPressEvent(e);
    }
}

void FileAssocDialog::takeFocusAwayFromEditor(bool up_or_down)
{
    nextPrevWidgetFocus(up_or_down);
    if(command_editor)
    {
        command_editor->clearFocus();
    }
}

void FileAssocDialog::loadFileAssociations()
{
    if(!db)
    {
        cerr << "fileassoc.o: Ha Ha Ha. Very funny" << endl;
        return;
    }
    
    QString q_string = QString("SELECT intid, extension, playcommand, "
                               "f_ignore, use_default "
                               "FROM videotypes ;");
    
    QSqlQuery a_query(q_string, db);
    if(a_query.isActive() && a_query.numRowsAffected() > 0)
    {
        while(a_query.next())
        {
            FileAssociation *new_fa = new FileAssociation(db,
                                                          a_query.value(0).toInt(),
                                                          a_query.value(1).toString(),
                                                          a_query.value(2).toString(),
                                                          a_query.value(3).toBool(),
                                                          a_query.value(4).toBool());
            if(file_associations.count() < 1)
            {
                current_fa = new_fa;
            }
            file_associations.append(new_fa);
            
            //
            //  Don't delete new'd file association, as the pointers
            //  are now "owned" by file_associations pointer list.
            //                                                          
        }
    }
    else
    {
        cerr << "fileassoc.o: Couldn'g get any filetypes from your database." << endl;
    }
    
}

void FileAssocDialog::saveFileAssociations()
{
    for(uint i = 0; i < file_associations.count(); i++)
    {
        file_associations.at(i)->saveYourself();
    }
}

void FileAssocDialog::showCurrentFA()
{
    if(!current_fa)
    {
        if(extension_select)
        {
            extension_select->SetContext(-2);
        }
        if(command_editor)
        {
            command_editor->hide();
            command_hack->SetContext(-2);
        }
        if(default_check)
        {
            default_check->SetContext(-2);
        }
        if(ignore_check)
        {
            ignore_check->SetContext(-2);
        }
        if(delete_button)
        {
            delete_button->SetContext(-2);
        }
        UIType *current_widget = getCurrentFocusWidget();
        if(current_widget)
        {
            current_widget->looseFocus();
        }
        if(new_button)
        {
            new_button->takeFocus();
            widget_with_current_focus = new_button;
        }
        else if(done_button)
        {
            done_button->takeFocus();
            widget_with_current_focus = done_button;
        }
        else
        {
            assignFirstFocus();
        }
    }
    else
    {
        if(extension_select)
        {
            extension_select->SetContext(-1);
            extension_select->cleanOut();
            for(uint i = 0; i < file_associations.count(); i++)
            {
                extension_select->addItem(file_associations.at(i)->getID(),
                                          file_associations.at(i)->getExtension());
            }
            extension_select->setToItem(current_fa->getID());
        }
    
        if(command_editor)
        {
            command_hack->SetContext(-1);
            command_editor->show();
            command_editor->setText(current_fa->getCommand());
        }
        if(default_check)
        {
            default_check->SetContext(-1);
            default_check->setState(current_fa->getDefault());
        }
        if(ignore_check)
        {
            ignore_check->SetContext(-1);
            ignore_check->setState(current_fa->getIgnore());
        }
        if(delete_button)
        {
            delete_button->SetContext(-1);
        }
    }
    update();
}

void FileAssocDialog::switchToFA(int which_one)
{
    for(uint i = 0; i < file_associations.count(); i++)
    {
        if(file_associations.at(i)->getID() == which_one)
        {
            current_fa = file_associations.at(i);
            i = file_associations.count() + 1;
        }
    }
    showCurrentFA();
}

void FileAssocDialog::saveAndExit()
{
    saveFileAssociations();
    done(0);
}

void FileAssocDialog::toggleDefault(bool yes_or_no)
{
    if(current_fa)
    {
        current_fa->setDefault(yes_or_no);
        current_fa->setChanged();
    }
}

void FileAssocDialog::toggleIgnore(bool yes_or_no)
{
    if(current_fa)
    {
        current_fa->setIgnore(yes_or_no);
        current_fa->setChanged();
    }
}

void FileAssocDialog::setPlayerCommand(QString new_command)
{
    if(current_fa)
    {
        if(current_fa->getCommand() != new_command)
        {
            current_fa->setCommand(new_command);
            current_fa->setChanged();
        }
    }
}

void FileAssocDialog::deleteCurrent()
{
    if(current_fa)
    {
        current_fa->deleteYourselfFromDB();
        file_associations.remove(current_fa);
        current_fa = file_associations.first();
    }
    showCurrentFA();
}

void FileAssocDialog::makeNewExtension()
{
    //
    //  Create the popup window for new extension
    //  dialog/widgets
    //

    new_extension_popup = new MythPopupBox(gContext->GetMainWindow(), "new extension popup");
    gContext->ThemeWidget(new_extension_popup);
    //new_extension_popup->setPaletteForegroundColor(popupHighlight);

    QLabel *above_filler = new QLabel("", new_extension_popup);
    QLabel *message = new QLabel("Please enter the new extension:", new_extension_popup);
    QLabel *below_filler = new QLabel("", new_extension_popup);

    above_filler->setBackgroundOrigin(ParentOrigin);
    message->setBackgroundOrigin(ParentOrigin);
    below_filler->setBackgroundOrigin(ParentOrigin);
        
    
    new_extension_editor = new MythRemoteLineEdit(new_extension_popup);

    MythPushButton *create_button = new MythPushButton("Create new extension", new_extension_popup);
    MythPushButton *nevermind_button = new MythPushButton("Cancel", new_extension_popup); 
    
    
    new_extension_popup->addWidget(above_filler, false);
    new_extension_popup->addWidget(message, false);
    new_extension_popup->addWidget(below_filler, false);
    new_extension_popup->addWidget(new_extension_editor, false);
    new_extension_popup->addWidget(create_button, false);
    new_extension_popup->addWidget(nevermind_button, false);
    
    new_extension_editor->setFocus();
    
    above_filler->adjustSize();
    message->adjustSize();
    below_filler->adjustSize();
    new_extension_editor->adjustSize();
    create_button->adjustSize();
    nevermind_button->adjustSize();
    
    new_extension_popup->polish();
    
    int pop_h = above_filler->height() + 
                message->height() + 
                below_filler->height() + 
                new_extension_editor->height() +
                create_button->height() + 
                nevermind_button->height() +
                (int)(110 * hmult);

    new_extension_popup->setMinimumHeight(pop_h);

    int max_w = message->width();
    if(new_extension_editor->width() > max_w)
    {
        max_w = new_extension_editor->width();
    }
    if(create_button->width() > max_w)
    {
        max_w = create_button->width();
    }
    if(nevermind_button->width() > max_w)
    {
        max_w = nevermind_button->width();
    }
    
    max_w += (int)(80 * wmult);

    int x = (int)(width() / 2) - (int)(max_w / 2);
    int y = (int)(height() / 2) - (int)(pop_h / 2);
    
    new_extension_popup->setFixedSize(max_w, pop_h);
    new_extension_popup->setGeometry(x, y, max_w, pop_h);

    QAccel *popaccel = new QAccel(new_extension_popup);
    popaccel->connectItem(popaccel->insertItem(Key_Escape), this,
                              SLOT(removeExtensionPopup()));
    connect(create_button, SIGNAL(pressed()), this, SLOT(createExtension()));
    connect(nevermind_button, SIGNAL(pressed()), this, SLOT(removeExtensionPopup()));
    
    new_extension_popup->Show();
    
    
}

void FileAssocDialog::createExtension()
{
    QString new_extension = new_extension_editor->text();
    if(new_extension.length() > 0)
    {
        FileAssociation *new_fa = new FileAssociation(db, new_extension);
        file_associations.append(new_fa);
        current_fa = new_fa;
    }
    removeExtensionPopup();
    showCurrentFA();
}

void FileAssocDialog::removeExtensionPopup()
{
    new_extension_popup->close();
    delete new_extension_editor;
    new_extension_editor = NULL;
    delete new_extension_popup;
    new_extension_popup = NULL;
    
    //
    //  Move focus to the extension
    //  selector
    //

    UIType *current_widget = getCurrentFocusWidget();
    if(current_widget)
    {
        current_widget->looseFocus();
    }
    if(extension_select)
    {
        widget_with_current_focus = extension_select;
        extension_select->takeFocus();
    }
    else
    {
        assignFirstFocus();
    }
    update();
}

void FileAssocDialog::wireUpTheme()
{
    extension_select = getUISelectorType("extension_select");
    if(extension_select)
    {
        connect(extension_select, SIGNAL(pushed(int)), this, SLOT(switchToFA(int)));
    }
    command_hack = getUIBlackHoleType("command_hack");
    if(command_hack)
    {
        command_hack->allowFocus(true);
        QFont f( "arial", 14, QFont::Bold);
        command_editor = new MythRemoteLineEdit(&f, this);
        command_editor->setFocusPolicy(QWidget::NoFocus);
        command_editor->setGeometry(command_hack->getScreenArea());
        connect(command_hack, SIGNAL(takingFocus()), 
                command_editor, SLOT(setFocus()));
        connect(command_editor, SIGNAL(tryingToLooseFocus(bool)), 
                this, SLOT(takeFocusAwayFromEditor(bool)));
        connect(command_editor, SIGNAL(textChanged(QString)),
                this, SLOT(setPlayerCommand(QString)));
    }

    default_check = getUICheckBoxType("default_check");
    if(default_check)
    {
        connect(default_check, SIGNAL(pushed(bool)), this, SLOT(toggleDefault(bool)));
    }
    
    ignore_check = getUICheckBoxType("ignore_check");
    if(ignore_check)
    {
        connect(ignore_check, SIGNAL(pushed(bool)), this, SLOT(toggleIgnore(bool)));
    }

    done_button = getUITextButtonType("done_button");
    if(done_button)
    {
        done_button->setText("Done");
        connect(done_button, SIGNAL(pushed()), this, SLOT(saveAndExit()));
    }

    new_button = getUITextButtonType("new_button");
    if(new_button)
    {
        new_button->setText("New");
        connect(new_button, SIGNAL(pushed()), this, SLOT(makeNewExtension()));
    }

    delete_button = getUITextButtonType("delete_button");
    if(delete_button)
    {
        delete_button->setText("Delete");
        connect(delete_button, SIGNAL(pushed()), this, SLOT(deleteCurrent()));
    }
    buildFocusList();
}

FileAssocDialog::~FileAssocDialog()
{
    file_associations.clear();
    if(command_editor)
    {
        delete command_editor;
    }
}

